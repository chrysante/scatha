#include "CodeGen/InterferenceGraph.h"

#include <iostream>

#include <graphgen/graphgen.h>
#include <range/v3/view.hpp>
#include <utl/graph.hpp>
#include <utl/strcat.hpp>
#include <utl/vector.hpp>

#include "Debug/DebugGraphviz.h"
#include "MIR/CFG.h"
#include "MIR/Instructions.h"

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
    auto nodeView = getNodeView<Node>();
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
        if (reg->fixed() && !isa<mir::CalleeRegister>(reg)) {
            uint32_t const col = utl::narrow_cast<uint32_t>(reg->index());
            colors[node] = node->col = col;
            maxCol = std::max(maxCol, col + 1);
        }
    }
    /// Greedy coloring algorithm:
    for (auto* n: lexOrdering | ranges::views::reverse) {
        if (colors.contains(n) || isa<CalleeRegister>(n->reg())) {
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
        maxCol = std::max(maxCol, col + 1);
        colors.insert({ n, col });
        n->col = col;
    }
    numCols = maxCol;
}

void InterferenceGraph::computeImpl(Function& F) {
    this->F = &F;
    for (auto& reg: F.virtualRegisters()) {
        addRegister(&reg);
    }
    for (auto& reg: F.calleeRegisters()) {
        addRegister(&reg);
    }
    for (auto argRegs = F.virtualArgumentRegisters(); auto* r: argRegs) {
        addEdges(r, argRegs);
    }
    for (auto retRegs = F.virtualReturnValueRegisters(); auto* r: retRegs) {
        addEdges(r, retRegs);
    }
    for (auto& BB: F) {
        auto live = BB.liveOut();
        for (auto& inst: BB | ranges::views::reverse) {
            for (auto* dest: inst.destRegisters()) {
                addEdges(dest, live);
            }
            /// cmov instruction may not write to the dest register
            if (!isa<CondCopyInst>(inst)) {
                for (auto* dest: inst.destRegisters()) {
                    live.erase(dest);
                }
            }
            for (auto* op: inst.operands() | Filter<Register>) {
                live.insert(op);
            }
        }
    }
}

void InterferenceGraph::addRegister(Register* reg) {
    auto nodeOwner = std::make_unique<Node>(reg);
    auto* node = nodeOwner.get();
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

static std::string toRegLetter(Register const* reg) {
    // clang-format off
    return SC_MATCH (*reg) {
        [](SSARegister const&) { return "S"; },
        [](VirtualRegister const&) { return "V"; },
        [](CalleeRegister const&) { return "C"; },
        [](HardwareRegister const&) { return "H"; },
    }; // clang-format on
}

void cg::generateGraphviz(InterferenceGraph const& graph,
                          std::ostream& ostream) {
    using namespace graphgen;
    auto* G = Graph::make(ID(0));
    utl::hashset<std::pair<ID, ID>> edges;
    for (auto* node: graph) {
        auto* vertex =
            Vertex::make(ID(node))->label(utl::strcat(toRegLetter(node->reg()),
                                                      node->reg()->index(),
                                                      " → H",
                                                      node->color()));
        G->add(vertex);
        for (auto* neighbour: node->neighbours()) {
            Edge edge{ ID(node), ID(neighbour) };
            if (edges.insert({ edge.from, edge.to }).second &&
                !edges.contains({ edge.to, edge.from }))
            {
                G->add(edge);
            }
        }
    }
    Graph H;
    H.label(std::string(graph.function()->name()));
    H.kind(graphgen::GraphKind::Undirected);
    H.add(G);
    H.font("SF Mono");
    generate(H, ostream);
}

void cg::generateGraphvizTmp(InterferenceGraph const& graph) {
    try {
        auto [path, file] =
            debug::newDebugFile(std::string(graph.function()->name()));
        generateGraphviz(graph, file);
        file.close();
        debug::createGraphAndOpen(path);
    }
    catch (std::exception const& e) {
        std::cout << e.what() << std::endl;
    }
}
