#include "CodeGen/SelectionDAG.h"

#include <exception>
#include <fstream>
#include <iostream>
#include <sstream>

#include <graphgen/graphgen.h>
#include <range/v3/algorithm.hpp>
#include <termfmt/termfmt.h>
#include <utl/stack.hpp>

#include "Common/PrintUtil.h"
#include "Debug/DebugGraphviz.h"
#include "IR/CFG.h"
#include "IR/Print.h"
#include "MIR/CFG.h"
#include "MIR/Print.h"
#include "Opt/Common.h"

using namespace scatha;
using namespace cg;

static bool hasSideEffects(ir::Instruction const& inst) {
    return opt::hasSideEffects(&inst) || isa<ir::TerminatorInst>(inst);
}

///
static bool isCritical(ir::Instruction const& inst) {
    return ::hasSideEffects(inst) || isa<ir::Load>(inst);
}

static bool isOutput(ir::Instruction const& inst) {
    return ranges::any_of(inst.users(), [&](auto* user) {
        return user->parent() != inst.parent();
    });
}

/// The following instructions are (potentially) critical:
/// - `Load`
/// - `Store`
/// - `Call`
/// - `Terminator`

SelectionDAG SelectionDAG::Build(ir::BasicBlock const& BB) {
    SelectionDAG DAG;
    DAG.BB = &BB;
    SelectionNode* lastWrite = nullptr;
    SelectionNode* lastCritical = nullptr;
    for (auto& inst: BB) {
        auto* instNode = DAG.get(&inst);
        DAG.all.insert(instNode);
        if (::isCritical(inst)) {
            // clang-format off
            SC_MATCH (inst) {
                [&](ir::Load const&) {
                    if (lastWrite) {
                        instNode->addExecutionDependency(lastWrite);
                    }
                    lastCritical = instNode;
                },
                [&](ir::Store const&) {
                    if (lastCritical) {
                        instNode->addExecutionDependency(lastCritical);
                    }
                    lastWrite = instNode;
                    lastCritical = instNode;
                },
                [&](ir::Call const&) {
                    if (lastCritical) {
                        instNode->addExecutionDependency(lastCritical);
                    }
                    lastWrite = instNode;
                    lastCritical = instNode;
                },
                [&](ir::TerminatorInst const&) {
                    if (lastCritical) {
                        instNode->addExecutionDependency(lastCritical);
                    }
                },
                [&](ir::Instruction const&) { SC_UNREACHABLE(); },
            }; // clang-format on
        }
        if (::hasSideEffects(inst)) {
            DAG.sideEffects.insert(instNode);
        }
        if (::isOutput(inst)) {
            DAG.outputs.insert(instNode);
        }
        auto sameBlock = [&](auto* other) {
            return inst.parent() == other->parent();
        };
        for (auto* operand: inst.operands() | Filter<ir::Instruction> |
                                ranges::views::filter(sameBlock))
        {
            auto* opNode = DAG.get(operand);
            instNode->addValueDependency(opNode);
        }
    }
    /// Add execution dependencies from the terminator to all output nodes
    auto* termNode = DAG.get(BB.terminator());
    for (auto* outputNode: DAG.outputs) {
        termNode->addExecutionDependency(outputNode);
    }
    /// Do a DFS over the DAG to identify the execution dependency sets
    utl::stack<SelectionNode*> stack;
    auto DFS = [&](auto& DFS, SelectionNode* node) -> void {
        for (auto* upstream: stack) {
            DAG.execDeps[upstream].insert(node);
        }
        stack.push(node);
        for (auto* dep: node->executionDependencies()) {
            DFS(DFS, dep);
        }
        stack.pop();
    };
    DFS(DFS, termNode);
    return DAG;
}

SelectionNode const* SelectionDAG::operator[](
    ir::Instruction const* inst) const {
    auto itr = map.find(inst);
    SC_ASSERT(itr != map.end(), "Not found");
    return itr->second.get();
}

SelectionNode const* SelectionDAG::root() const {
    return (*this)[basicBlock()->terminator()];
}

utl::small_vector<SelectionNode*> SelectionDAG::topsort() {
    utl::small_vector<SelectionNode*> result;
    utl::hashset<SelectionNode const*> marked;
    auto DFS = [&](auto DFS, SelectionNode* node) {
        if (marked.contains(node)) {
            return;
        }
        for (auto* dep: node->dependencies()) {
            DFS(DFS, dep);
        }
        marked.insert(node);
        result.push_back(node);
    };
    DFS(DFS, root());
    ranges::reverse(result);
    return result;
}

void SelectionDAG::erase(SelectionNode* node) {
    node->erase();
    all.erase(node);
}

SelectionNode* SelectionDAG::get(ir::Instruction const* inst) {
    auto& ptr = map[inst];
    if (!ptr) {
        ptr = std::make_unique<SelectionNode>(inst);
    }
    return ptr.get();
}

using namespace graphgen;

static Label makeIRLabel(SelectionDAG const& DAG, ir::Instruction const* inst) {
    std::stringstream sstr;
    tfmt::setHTMLFormattable(sstr);
    ir::printDecl(*inst, sstr);
    return Label(std::move(sstr).str(), LabelKind::HTML);
}

static Label makeMIRLabel(SelectionDAG const& DAG, SelectionNode const* node) {
    std::stringstream sstr;
    tfmt::setHTMLFormattable(sstr);
    sstr << tableBegin() << "\n";
    if (auto name = node->irInst()->name(); !name.empty()) {
        sstr << rowBegin() << name << ":" << rowEnd() << "\n";
    }
    for (auto& inst: node->mirInstructions()) {
        sstr << rowBegin();
        mir::print(inst, sstr);
        sstr << rowEnd() << "\n";
    }
    sstr << tableEnd() << "\n";
    return Label(std::move(sstr).str(), LabelKind::HTML);
}

static Label makeLabel(SelectionDAG const& DAG, SelectionNode const* node) {
    if (node->matched()) {
        return makeMIRLabel(DAG, node);
    }
    else {
        return makeIRLabel(DAG, node->irInst());
    }
}

void cg::generateGraphviz(SelectionDAG const& DAG, std::ostream& ostream) {
    auto* G = Graph::make(ID(0));
    for (auto* node: DAG.nodes()) {
        auto* vertex = Vertex::make(ID(node))->label(makeLabel(DAG, node));
        /// Add all use edges
        for (auto* dependency: node->valueDependencies()) {
            G->add(Edge{ ID(node), ID(dependency), .style = Style::Dashed });
        }
        /// Add all 'execution' edges
        for (auto* dependency: node->executionDependencies()) {
            G->add(Edge{ ID(node),
                         ID(dependency),
                         .color = Color::Magenta,
                         .style = Style::Bold });
        }
        if (!node->matched()) {
            vertex->color(Color::Red);
            vertex->style(Style::Bold);
        }
        G->add(vertex);
    }
    Graph H;
    H.label(std::string(DAG.basicBlock()->name()));
    H.add(G);
    H.font("SF Mono");
    H.rankdir(RankDir::BottomTop);
    generate(H, ostream);
}

void cg::generateGraphvizTmp(SelectionDAG const& DAG, std::string name) {
    try {
        auto [path, file] = debug::newDebugFile(std::move(name));
        generateGraphviz(DAG, file);
        file.close();
        debug::createGraphAndOpen(path);
    }
    catch (std::exception const& e) {
        std::cout << e.what() << std::endl;
    }
}

void cg::generateGraphvizTmp(SelectionDAG const& DAG) {
    generateGraphvizTmp(DAG, std::string(DAG.basicBlock()->name()));
}
