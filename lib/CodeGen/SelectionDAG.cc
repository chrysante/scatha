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
using namespace ir;

///
static bool isCritical(ir::Instruction const& inst) {
    return opt::hasSideEffects(&inst) || isa<Load>(inst) ||
           isa<TerminatorInst>(inst);
}

static bool isOutput(ir::Instruction const& inst) {
    return ranges::any_of(inst.users(), [&](auto* user) {
        return user->parent() != inst.parent();
    });
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
        for (auto* operand: inst.operands()) {
            if (isa<BasicBlock>(operand) || isa<Callable>(operand)) {
                continue;
            }
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

SelectionNode* SelectionDAG::get(ir::Value const* value) {
    auto& ptr = map[value];
    if (!ptr) {
        ptr = allocate<SelectionNode>(allocator, value);
    }
    return ptr;
}

using namespace graphgen;

static Label makeIRLabel(SelectionDAG const& DAG, ir::Value const* value) {
    std::stringstream sstr;
    if (auto* inst = dyncast<ir::Instruction const*>(value);
        !inst || inst->parent() == DAG.basicBlock())
    {
        tfmt::setHTMLFormattable(sstr);
        ir::printDecl(*value, sstr);
    }
    else {
        sstr << "<font color=\"Silver\">";
        ir::printDecl(*value, sstr);
        sstr << "</font>";
    }
    return Label(std::move(sstr).str(), LabelKind::HTML);
}

static Label makeMIRLabel(SelectionDAG const& DAG, SelectionNode const* node) {
    mir::Instruction const* inst = node->mirInstruction();
    std::stringstream sstr;
    tfmt::setHTMLFormattable(sstr);
    if (auto name = node->irValue()->name(); !name.empty()) {
        sstr << name << ":\n";
    }
    mir::print(*inst, sstr);
    return Label(std::move(sstr).str(), LabelKind::HTML);
}

static Label makeLabel(SelectionDAG const& DAG, SelectionNode const* node) {
    if (node->mirInstruction()) {
        return makeMIRLabel(DAG, node);
    }
    else {
        return makeIRLabel(DAG, node->irValue());
    }
}

/// \Returns `true` if the node is critical for visualization
static bool vizCritical(SelectionNode const* node) {
    auto* value = node->irValue();
    return !isa<Parameter>(value) && !isa<Constant>(value) &&
           !isa<Alloca>(value);
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
        if (node->mirInstruction()) {
            vertex->color(Color::Green);
            vertex->style(Style::Bold);
        }
        G->add(vertex);
    }
    Graph H;
    H.label(makeIRLabel(DAG, DAG.basicBlock()));
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
