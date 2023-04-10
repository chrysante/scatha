#include "CodeGen/InterferenceGraph.h"

#include "MIR/CFG.h"

using namespace scatha;
using namespace cg;
using namespace mir;

InterferenceGraph InterferenceGraph::compute(Function& function) {
    InterferenceGraph result;
    result.computeImpl(function);
    return result;
}

void InterferenceGraph::colorize(size_t maxColors) {
    numCols = maxColors;
    for (auto& n: nodes) {
        n->col = 0;
    }
}

void InterferenceGraph::computeImpl(Function& F) {
    for (auto* reg: F.registers()) {
        addRegister(reg);
    }
    for (auto* reg: F.virtualRegisters()) {
        addRegister(reg);
    }
    for (auto& BB: F) {
        auto live = BB.liveOut();
        for (auto& inst: BB | ranges::views::reverse) {
            auto* dest = inst.dest();
            if (dest) {
                addEdges(dest, live);
            }
            for (auto* op: inst.operands()) {
                auto* reg = op ? dyncast<Register*>(op) : nullptr;
                if (!reg) {
                    continue;
                }
                live.insert(reg);
            }
            live.erase(dest);
        }
    }
}

void InterferenceGraph::addRegister(Register* reg) {
    auto nodeOwner = std::make_unique<Node>(reg);
    auto* node     = nodeOwner.get();
    nodes.push_back(std::move(nodeOwner));
    regMap.insert({ reg, node });
}

void InterferenceGraph::addEdges(mir::Register* reg, auto&& listOfRegs) {
    Node* regNode = find(reg);
    for (auto* rhs: listOfRegs) {
        Node* rhsNode = find(rhs);
        if (regNode == rhsNode) {
            continue;
        }
        regNode->addNeighbour(rhsNode);
        rhsNode->addNeighbour(regNode);
    }
}

InterferenceGraph::Node* InterferenceGraph::find(mir::Register* reg) {
    auto itr = regMap.find(reg);
    SC_ASSERT(itr != regMap.end(), "Not found");
    return itr->second;
}
