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

static uint32_t firstAvail(utl::hashset<uint32_t> const& used) {
    for (uint32_t i = 0;; ++i) {
        if (!used.contains(i)) {
            return i;
        }
    }
}

void InterferenceGraph::colorize() {
    utl::small_vector<Node*> lexOrdering;
    lexOrdering.reserve(size());
    auto neighbours = [](Node* node) { return node->neighbours(); };
    auto nodeView   = getNodeView<Node>();
    utl::find_lex_ordering(nodeView.begin(),
                           nodeView.end(),
                           neighbours,
                           std::back_inserter(lexOrdering));
    bool const isChordal =
        utl::is_chordal(lexOrdering.begin(), lexOrdering.end(), neighbours);
    /// ** Graph is not chordal for some reason... **
    (void)isChordal;
    utl::hashmap<Node const*, uint32_t> colors;
    uint32_t maxCol = 0;
    for (auto* node: nodeView) {
        auto* reg = node->reg();
        if (reg->fixed()) {
            uint32_t const col = utl::narrow_cast<uint32_t>(reg->index());
            colors[node] = node->col = col;
            maxCol                   = std::max(maxCol, col + 1);
        }
    }
    /// Greedy coloring algorithm:
    for (auto* n: lexOrdering) {
        if (colors.contains(n)) {
            continue;
        }
        utl::hashset<uint32_t> used;
        for (auto m: n->neighbours()) {
            auto itr = colors.find(m);
            if (itr != colors.end()) {
                used.insert(itr->second);
            }
        }
        uint32_t col = firstAvail(used);
        maxCol       = std::max(maxCol, col + 1);
        colors.insert({ n, col });
        n->col = col;
    }
    numCols = maxCol;
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
