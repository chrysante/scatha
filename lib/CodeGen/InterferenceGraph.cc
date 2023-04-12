#include "CodeGen/InterferenceGraph.h"

#include <utl/graph.hpp>
#include <utl/vector.hpp>

#include "MIR/CFG.h"

using namespace scatha;
using namespace cg;
using namespace mir;

InterferenceGraph InterferenceGraph::compute(Function& function) {
    InterferenceGraph result;
    result.computeImpl(function);
    return result;
}

void InterferenceGraph::colorize() {
    utl::small_vector<Node const*> lexOrdering;
    lexOrdering.reserve(size());
    auto neighbours = [](Node const* node) { return node->neighbours(); };
    utl::find_lex_ordering(begin(),
                           end(),
                           neighbours,
                           std::back_inserter(lexOrdering));
    bool const isChordal =
        utl::is_chordal(lexOrdering.begin(), lexOrdering.end(), neighbours);
    /// ** Graph is not chordal for some reason... **
    (void)isChordal;
    numCols = utl::greedy_color(lexOrdering.begin(),
                                lexOrdering.end(),
                                neighbours,
                                [](Node const* node, size_t color) {
        const_cast<Node*>(node)->col = color;
    });
}

void InterferenceGraph::computeImpl(Function& F) {
    for (auto& reg: F.virtualRegisters()) {
        addRegister(&reg);
    }
    for (auto argRegs = F.virtualArgumentRegisters(); auto* r: argRegs) {
        addEdges(r, argRegs);
    }
    for (auto returnRegs = F.virtualReturnValueRegisters(); auto* r: returnRegs)
    {
        addEdges(r, returnRegs);
    }
    for (auto& BB: F) {
        auto live = BB.liveOut();
        for (auto& inst: BB | ranges::views::reverse) {
            auto* dest = inst.dest();
            if (dest && isa<VirtualRegister>(dest)) {
                addEdges(dest, live);
            }
            for (auto* op: inst.operands()) {
                auto* vreg = op ? dyncast<VirtualRegister*>(op) : nullptr;
                if (!vreg) {
                    continue;
                }
                live.insert(vreg);
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
