#include "CodeGen/SelectionDAG.h"

#include <exception>
#include <fstream>
#include <iostream>
#include <sstream>

#include <graphgen/graphgen.h>
#include <termfmt/termfmt.h>

#include "Debug/DebugGraphviz.h"
#include "IR/CFG.h"
#include "IR/Print.h"
#include "MIR/CFG.h"
#include "MIR/Print.h"
#include "Opt/Common.h"

using namespace scatha;
using namespace cg;

///
static bool isCritical(ir::Instruction const& inst) {
    return opt::hasSideEffects(&inst) || isa<ir::Load>(inst) ||
           isa<ir::TerminatorInst>(inst);
}

static bool isOutput(ir::Instruction const& inst) {
    return ranges::any_of(inst.users(), [&](auto* user) {
        return user->parent() != inst.parent();
    });
}

SelectionNode::SelectionNode(ir::Instruction const* inst): _irInst(inst) {}

SelectionNode::~SelectionNode() = default;

void SelectionNode::setMIR(mir::SSARegister* reg,
                           List<mir::Instruction> insts) {
    _register = reg;
    _mirInsts = std::move(insts);
    _matched = true;
}

static void copyDependencies(SelectionNode& oldDepender,
                             SelectionNode& newDepender) {
    auto impl = [&](auto get, auto add) {
        for (auto* dependency: std::invoke(get, oldDepender) | ToSmallVector<>)
        {
            std::invoke(add, newDepender, dependency);
        }
    };
    impl([](auto& node) { return node.valueDependencies(); },
         &SelectionNode::addValueDependency);
    impl([](auto& node) { return node.executionDependencies(); },
         &SelectionNode::addExecutionDependency);
}

void SelectionNode::merge(SelectionNode& child) {
    copyDependencies(child, *this);
    removeDependency(&child);
}

SelectionDAG SelectionDAG::Build(ir::BasicBlock const& BB) {
    SelectionDAG DAG;
    DAG.BB = &BB;
    utl::small_vector<SelectionNode*> orderedCritical;
    for (auto& inst: BB) {
        auto* instNode = DAG.get(&inst);
        if (::isCritical(inst)) {
            DAG.sideEffects.insert(instNode);
            orderedCritical.push_back(instNode);
        }
        if (::isOutput(inst)) {
            DAG.outputs.insert(instNode);
        }
        for (auto* operand: inst.operands() | Filter<ir::Instruction>) {
            auto* opNode = DAG.get(operand);
            instNode->addValueDependency(opNode);
        }
    }
    for (auto [A, B]:
         ranges::views::zip(orderedCritical,
                            orderedCritical | ranges::views::drop(1)))
    {
        B->addExecutionDependency(A);
    }
    auto* termNode = DAG.get(BB.terminator());
    for (auto* outputNode: DAG.outputs) {
        termNode->addExecutionDependency(outputNode);
    }
    return DAG;
}

SelectionNode const* SelectionDAG::operator[](
    ir::Instruction const* inst) const {
    auto itr = map.find(inst);
    SC_ASSERT(itr != map.end(), "Not found");
    return itr->second;
}

SelectionNode const* SelectionDAG::root() const {
    return (*this)[basicBlock()->terminator()];
}

SelectionNode* SelectionDAG::get(ir::Instruction const* inst) {
    auto& ptr = map[inst];
    if (!ptr) {
        ptr = allocate<SelectionNode>(allocator, inst);
    }
    return ptr;
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
    if (auto name = node->irInst()->name(); !name.empty()) {
        sstr << name << ":\n";
    }
    for (auto& inst: node->mirInstructions()) {
        mir::print(inst, sstr);
    }
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

/// \Returns `true` if the node is critical for visualization
static bool vizCritical(SelectionNode const* node) {
    return !isa<ir::Alloca>(node->irInst());
}

void cg::generateGraphviz(SelectionDAG const& DAG, std::ostream& ostream) {
    auto* G = Graph::make(ID(0));
    for (auto* node: DAG.nodes() | ranges::views::filter(vizCritical)) {
        auto* vertex = Vertex::make(ID(node))->label(makeLabel(DAG, node));
        /// Add all use edges
        for (auto* dependency:
             node->valueDependencies() | ranges::views::filter(vizCritical))
        {
            G->add(Edge{ ID(node), ID(dependency), .style = Style::Dashed });
        }
        /// Add all 'execution' edges
        for (auto* dependency:
             node->executionDependencies() | ranges::views::filter(vizCritical))
        {
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
