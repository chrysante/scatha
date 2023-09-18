#include "CodeGen/SelectionDAG.h"

#include <sstream>

#include <graphgen/graphgen.h>
#include <termfmt/termfmt.h>

#include "IR/CFG.h"
#include "IR/Print.h"

using namespace scatha;
using namespace cg;
using namespace ir;

SelectionDAG SelectionDAG::build(ir::BasicBlock& BB) {
    SelectionDAG DAG;
    DAG.BB = &BB;
    /// We start the index at 1 because all values that are not defined in this
    /// block have index 0
    ssize_t index = 1;
    for (auto& inst: BB) {
        auto* instNode = DAG.get(&inst);
        instNode->setIndex(index++);
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

SelectionNode const* SelectionDAG::operator[](ir::Instruction* inst) const {
    auto itr = nodemap.find(inst);
    SC_ASSERT(itr != nodemap.end(), "Not found");
    return itr->second;
}

SelectionNode* SelectionDAG::get(ir::Value* value) {
    auto& ptr = nodemap[value];
    if (!ptr) {
        ptr = allocate<SelectionNode>(allocator, value);
    }
    return ptr;
}

using namespace graphgen;

static Label makeLabel(ir::Value const* value) {
    std::stringstream sstr;
    tfmt::setHTMLFormattable(sstr);
    ir::printDecl(*value, sstr);
    return Label(std::move(sstr).str(), LabelKind::HTML);
}

void cg::generateGraphviz(SelectionDAG const& DAG, std::ostream& ostream) {
    Graph G;
    G.font("SF Mono");
    G.rankdir(RankDir::BottomTop);
    for (auto* node: DAG.nodes()) {
        auto* vertex = Vertex::make(ID(node))->label(makeLabel(node->value()));
        for (auto* opNode: node->operands()) {
            G.add(Edge{ ID(node), ID(opNode) });
        }
        G.add(vertex);
    }
    generate(G, ostream);
}
