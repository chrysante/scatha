#include "CodeGen/SelectionDAG.h"

#include <sstream>

#include <graphgen/graphgen.h>
#include <termfmt/termfmt.h>

#include "Common/Ranges.h"
#include "IR/CFG.h"
#include "IR/Print.h"

using namespace scatha;
using namespace cg;
using namespace ir;

SelectionDAG SelectionDAG::build(ir::BasicBlock& BB) {
    SelectionDAG DAG;
    DAG.BB = &BB;
    for (auto& inst: BB) {
        DAG.nodemap[&inst] = SelectionNode(&inst);
    }
    for (auto& inst: BB) {
        auto* instNode = DAG[&inst];
        for (auto* user: inst.users()) {
            if (auto* userNode = DAG.find(user)) {
                instNode->addPredecessor(userNode);
            }
        }
        for (auto* operand: inst.operands() | Filter<Instruction>) {
            if (auto* opNode = DAG.find(operand)) {
                instNode->addSuccessor(opNode);
            }
        }
    }
    return DAG;
}

SelectionNode const* SelectionDAG::operator[](ir::Instruction* inst) const {
    auto* node = find(inst);
    SC_ASSERT(node, "Not found");
    return node;
}

SelectionNode const* SelectionDAG::find(ir::Instruction* inst) const {
    auto itr = nodemap.find(inst);
    if (itr != nodemap.end()) {
        return &itr->second;
    }
    return nullptr;
}

using namespace graphgen;

static Label makeLabel(ir::Instruction const* inst) {
    std::stringstream sstr;
    tfmt::setHTMLFormattable(sstr);
    ir::print(*inst, sstr);
    return Label(std::move(sstr).str(), LabelKind::HTML);
}

void cg::generateGraphviz(SelectionDAG const& DAG, std::ostream& ostream) {
    Graph G;
    G.font("SF Mono");
    G.rankdir(RankDir::BottomTop);
    for (auto& node: DAG.nodes()) {
        auto* vertex =
            Vertex::make(ID(&node))->label(makeLabel(node.instruction()));
        for (auto* opNode: node.operands()) {
            G.add(Edge{ ID(&node), ID(opNode) });
        }
        G.add(vertex);
    }
    generate(G, ostream);
}
