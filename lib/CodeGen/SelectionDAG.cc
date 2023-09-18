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
#include "Opt/Common.h"

using namespace scatha;
using namespace cg;
using namespace ir;

SelectionDAG SelectionDAG::build(ir::BasicBlock const& BB) {
    SelectionDAG DAG;
    DAG.BB = &BB;
    /// We start the index at 1 because all values that are not defined in this
    /// block have index 0
    ssize_t index = 1;
    for (auto& inst: BB) {
        auto* instNode = DAG.get(&inst);
        instNode->setIndex(index++);
        if (opt::hasSideEffects(&inst)) {
            DAG.critical.push_back(&inst);
        }
        for (auto* operand: inst.operands()) {
            if (isa<BasicBlock>(operand) || isa<Callable>(operand)) {
                continue;
            }
            auto* opNode = DAG.get(operand);
            instNode->addSuccessor(opNode);
            opNode->addPredecessor(instNode);
        }
    }
    return DAG;
}

SelectionNode const* SelectionDAG::operator[](
    ir::Instruction const* inst) const {
    auto itr = nodemap.find(inst);
    SC_ASSERT(itr != nodemap.end(), "Not found");
    return itr->second;
}

SelectionNode* SelectionDAG::get(ir::Value const* value) {
    auto& ptr = nodemap[value];
    if (!ptr) {
        ptr = allocate<SelectionNode>(allocator, value);
    }
    return ptr;
}

using namespace graphgen;

static Label makeLabel(SelectionDAG const& DAG, ir::Value const* value) {
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

void cg::generateGraphviz(SelectionDAG const& DAG, std::ostream& ostream) {
    auto* G = Graph::make(ID(0));
    for (auto* node: DAG.nodes()) {
        auto* vertex =
            Vertex::make(ID(node))->label(makeLabel(DAG, node->value()));
        for (auto* opNode: node->operands()) {
            G->add(Edge{ ID(node), ID(opNode) });
        }
        G->add(vertex);
    }
    Graph H;
    H.label(makeLabel(DAG, DAG.basicBlock()));
    H.add(G);
    H.font("SF Mono");
    H.rankdir(RankDir::BottomTop);
    generate(H, ostream);
}

void cg::generateGraphvizTmp(SelectionDAG const& DAG) {
    try {
        auto [path, file] =
            debug::newDebugFile(std::string(DAG.basicBlock()->name()));
        generateGraphviz(DAG, file);
        file.close();
        debug::createGraphAndOpen(path);
    }
    catch (std::exception const& e) {
        std::cout << e.what() << std::endl;
    }
}
