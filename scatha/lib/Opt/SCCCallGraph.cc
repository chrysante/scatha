#include "Opt/SCCCallGraph.h"

#include <iostream>

#include <graphgen/graphgen.h>
#include <utl/graph.hpp>
#include <utl/scope_guard.hpp>

#include "Common/Ranges.h"
#include "Debug/DebugGraphviz.h"
#include "IR/CFG.h"
#include "IR/Module.h"

using namespace scatha;
using namespace ir;
using namespace opt;
using namespace ranges::views;

using FunctionNode = SCCCallGraph::FunctionNode;
using SCCNode = SCCCallGraph::SCCNode;

utl::hashset<ir::Call*> const& FunctionNode::callsites(
    FunctionNode const& callee) const {
    auto itr = _callsites.find(&callee);
    SC_ASSERT(itr != _callsites.end(), "Not found");
    return itr->second;
}

SCCCallGraph::SCCNode::SCCNode(utl::small_vector<FunctionNode*> nodes):
    _nodes(std::move(nodes)) {
    for (auto* node: _nodes) {
        node->_sccAndIsLeaf.set_pointer(this);
    }
}

SCCCallGraph SCCCallGraph::compute(Module& mod) {
    SCCCallGraph result(mod);
    result.computeCallGraph();
    result.computeSCCs();
    return result;
}

SCCCallGraph SCCCallGraph::computeNoSCCs(Module& mod) {
    SCCCallGraph result(mod);
    result.computeCallGraph();
    return result;
}

void SCCCallGraph::computeCallGraph() {
    _nodes = *mod | ranges::views::transform([](Function& function) {
        return std::make_unique<FunctionNode>(&function);
    }) | ranges::to<std::vector>;
    for (auto& node: _nodes) {
        _funcMap[&node->function()] = node.get();
    }
    for (auto& function: *mod) {
        auto& thisNode = findMut(&function);
        bool haveNativeCalls = false;
        for (auto& call: function.instructions() | Filter<Call>) {
            haveNativeCalls |= !isa<ForeignFunction>(call.function());
            auto* target = dyncast<Function*>(call.function());
            if (!target) {
                continue;
            }
            /// We ignore self recursion, because the inliner handles it
            /// seperately
            if (target == &function) {
                continue;
            }
            auto& succNode = findMut(target);
            thisNode.addSuccessor(&succNode);
            succNode.addPredecessor(&thisNode);
            thisNode._callsites[&succNode].insert(&call);
        }
        thisNode._sccAndIsLeaf.set_integer(!haveNativeCalls);
    }
}

void SCCCallGraph::computeSCCs() {
    auto vertices = _nodes | ToAddress;
    utl::compute_sccs(vertices.begin(), vertices.end(),
                      [](FunctionNode* node) { return node->successors(); },
                      [&] { _sccs.push_back(std::make_unique<SCCNode>()); },
                      [&](FunctionNode* node) {
        auto& scc = *_sccs.back();
        scc.addNode(node);
    });
    /// After we have computed the SCC's, we set up parent pointers of the
    /// function nodes.
    for (auto& scc: _sccs) {
        for (auto* function: scc->_nodes) {
            function->_sccAndIsLeaf.set_pointer(scc.get());
        }
    }
    /// Then we set up the remaining links to make the set of SCC's into a graph
    /// representing the call graph.
    for (auto& scc: _sccs) {
        for (auto* function: scc->nodes()) {
            for (auto* succ: function->successors()) {
                auto& succSCC = succ->scc();
                if (&succSCC == scc.get()) {
                    continue;
                }
                scc->addSuccessor(&succSCC);
                succSCC.addPredecessor(scc.get());
            }
        }
    }
}

namespace {

struct SCC_DFS {
    FunctionNode const* from;
    FunctionNode const* to;
    SCCNode const* scc;
    utl::hashset<FunctionNode const*> visited;

    SCC_DFS(FunctionNode* from, FunctionNode* to):
        from(from), to(to), scc(&from->scc()) {}

    bool search() { return searchImpl(from); }

    bool searchImpl(FunctionNode const* current) {
        if (current == to) {
            return true;
        }
        if (!visited.insert(current).second) {
            return false;
        }
        for (auto* succ: from->successors()) {
            if (&succ->scc() == scc && searchImpl(succ)) {
                return true;
            }
        }
        return false;
    }
};

struct SplitSCC {
    FunctionNode* function;
    SCCNode* scc;

    SplitSCC(FunctionNode* function):
        function(function), scc(&function->scc()) {}

    std::unique_ptr<SCCNode> compute() {
        utl::hashset<FunctionNode*> result{ function };
        for (auto* succ: function->successors()) {
            computeImpl(succ, result);
        }
        auto nodes = result | ToSmallVector<>;
        return std::make_unique<SCCNode>(std::move(nodes));
    }

    void computeImpl(FunctionNode* current,
                     utl::hashset<FunctionNode*>& result) {
        if (&current->scc() != scc) {
            return;
        }
        if (current == function) {
            return;
        }
        result.insert(current);
        for (auto* succ: current->successors()) {
            computeImpl(succ, result);
        }
    }
};

} // namespace

/// Test if \p succ is a successor of \p pred by checking all nodes within the
/// scc for edges
static bool computeIsSuccessor(SCCNode const* pred, SCCNode const* succ) {
    for (auto* predFn: pred->nodes()) {
        for (auto* succFn: succ->nodes()) {
            if (predFn->isSuccessor(succFn)) {
                return true;
            }
        }
    }
    return false;
}

static void removeFromGraph(SCCNode* scc) {
    for (auto* succ: scc->successors()) {
        succ->removePredecessor(scc);
    }
    for (auto* pred: scc->predecessors()) {
        pred->removeSuccessor(scc);
    }
}

void SCCCallGraph::recomputeForwardEdges(SCCNode* scc) {
    for (auto* callerNode: scc->_nodes) {
        for (auto& call: callerNode->function().instructions() | Filter<Call>) {
            auto* callee = dyncast<Function*>(call.function());
            if (!callee) {
                continue;
            }
            auto* calleeNode = &findMut(callee);
            callerNode->addSuccessor(calleeNode);
            calleeNode->addPredecessor(callerNode);
            callerNode->_callsites[calleeNode].insert(&call);
            auto* calleeSCC = &calleeNode->scc();
            if (scc != calleeSCC) {
                scc->addSuccessor(calleeSCC);
                calleeSCC->addPredecessor(scc);
            }
        }
    }
}

void SCCCallGraph::recomputeBackEdges(SCCNode* oldSCC,
                                      std::span<SCCNode* const> newSCCs) {
    for (auto* predSCC: oldSCC->predecessors()) {
        utl::small_vector<bool> foundEdge(newSCCs.size());
        for (auto* predNode: predSCC->nodes()) {
            for (auto* succ: predNode->successors()) {
                for (auto [index, newSCC]: newSCCs | ranges::views::enumerate) {
                    if (&succ->scc() == newSCC) {
                        foundEdge[index] = true;
                    }
                }
                if (ranges::all_of(foundEdge, ranges::identity{})) {
                    goto loopEnd;
                }
            }
        }
loopEnd:
        for (auto [newSCC, isPred]: ranges::views::zip(newSCCs, foundEdge)) {
            if (isPred) {
                newSCC->addPredecessor(predSCC);
                predSCC->addSuccessor(newSCC);
            }
        }
    }
}

using Modification = SCCCallGraph::Modification;

Modification SCCCallGraph::removeCall(Function* caller, Function const* callee,
                                      Call const* callInst) {
    utl::scope_guard val = [&] { validate(); };
    auto& callerNode = findMut(caller);
    auto& calleeNode = findMut(callee);
    SC_ASSERT(callerNode.isSuccessor(&calleeNode),
              "Must be a successor to remove the edge");
    /// We remove `call` from our list of call sites
    auto& callsites = callerNode.mutCallsites(calleeNode);
    auto callItr = callsites.find(callInst);
    SC_ASSERT(callItr != callsites.end(), "");
    callsites.erase(callItr);
    /// If there are still calls to `callee` left, the structure of the call
    /// graph is unchanged
    if (!callsites.empty()) {
        return Modification::None;
    }
    /// Otherwise, `calleeNode` is no longer a successor of `callerNode`
    callerNode.removeSuccessor(&calleeNode);
    calleeNode.removePredecessor(&callerNode);
    /// Now we need to check if the removed edge split an SCC
    if (&callerNode.scc() != &calleeNode.scc()) {
        if (!computeIsSuccessor(&callerNode.scc(), &calleeNode.scc())) {
            /// Callees SCC is no longer a successor of the callers SCC
            callerNode.scc().removeSuccessor(&calleeNode.scc());
            calleeNode.scc().removePredecessor(&callerNode.scc());
        }
        return Modification::RemovedEdge;
    }
    /// Both nodes are in the same SCC. We perform two depth first searches
    /// _within the SCC_, to see if both nodes are still reachable from each
    /// other.
    if (SCC_DFS(&callerNode, &calleeNode).search() &&
        SCC_DFS(&calleeNode, &callerNode).search())
    {
        return Modification::RemovedEdge;
    }
    /// Create `oldSCC` reference here, because `SplitSCC` updates the SCC of
    /// `callerNode`
    auto& oldSCC = callerNode.scc();
    /// We split the SCC
    auto callerSCC = SplitSCC(&callerNode).compute();
    auto calleeSCC = SplitSCC(&calleeNode).compute();
    removeFromGraph(&oldSCC);
    recomputeForwardEdges(callerSCC.get());
    recomputeForwardEdges(calleeSCC.get());
    recomputeBackEdges(&oldSCC, std::array{ callerSCC.get(), calleeSCC.get() });
    oldSCC.clearEdges();
    oldSCC._nodes.clear();
    Modification result = { Modification::SplitSCC,
                            { callerSCC.get(), calleeSCC.get() } };
    _sccs.push_back(std::move(callerSCC));
    _sccs.push_back(std::move(calleeSCC));
    return result;
}

Modification SCCCallGraph::addCall(ir::Function* caller, ir::Function* callee,
                                   ir::Call* callInst) {
    if (caller == callee) {
        return Modification::None;
    }
    utl::scope_guard val = [&] { validate(); };
    auto& callerNode = findMut(caller);
    auto& calleeNode = findMut(callee);
    if (calleeNode.isPredecessor(&callerNode)) {
        auto& callsites = callerNode.mutCallsites(calleeNode);
        SC_ASSERT(!callsites.contains(callInst), "");
        callsites.insert(callInst);
        return Modification::None;
    }
    auto& callsites = callerNode._callsites[&calleeNode];
    callsites.insert(callInst);
    callerNode.addSuccessor(&calleeNode);
    calleeNode.addPredecessor(&callerNode);
    callerNode.scc().addSuccessor(&calleeNode.scc());
    calleeNode.scc().addPredecessor(&callerNode.scc());
    if (!SCC_DFS(&calleeNode, &callerNode).search()) {
        /// We did not introduce a cycle
        return Modification::AddedEdge;
    }
    SC_UNIMPLEMENTED();
}

void SCCCallGraph::RecomputeCalleesResult::merge(
    RecomputeCalleesResult const& rhs) {
    newCallees.insert(rhs.newCallees.begin(), rhs.newCallees.end());
    modifiedSCCs.insert(rhs.modifiedSCCs.begin(), rhs.modifiedSCCs.end());
}

SCCCallGraph::RecomputeCalleesResult SCCCallGraph::recomputeCallees(
    FunctionNode& node) {
    auto callsites = node._callsites | values | join |
                     ranges::to<utl::hashset<Call*>>;
    RecomputeCalleesResult result;
    for (auto& call: node.function().instructions() | Filter<Call>) {
        auto* callee = dyncast<Function*>(call.function());
        if (!callee) {
            continue;
        }
        auto itr = callsites.find(&call);
        if (itr != callsites.end()) {
            callsites.erase(itr);
            continue;
        }
        auto& calleeNode = findMut(callee);
        auto modResult = addCall(&node.function(), callee, &call);
        switch (modResult.type) {
        case Modification::AddedEdge:
            result.newCallees.insert(&calleeNode);
            break;
        case Modification::MergedSCCs:
            result.newCallees.insert(&calleeNode);
            result.modifiedSCCs.insert(modResult.modifiedSCCs.begin(),
                                       modResult.modifiedSCCs.end());
            break;
        default:
            break;
        }
    }
    /// Erase all call sites from the node that we didn't find in the function.
    /// We do this in this awkward fashion because the call instructions may
    /// have been deallocated at this point, so we cannot dereference them the
    /// get the callee
    for (auto& [callee, list]: node._callsites) {
        auto toErase = list | filter([&](auto* inst) {
            return callsites.contains(inst);
        }) | ToSmallVector<>;
        for (auto* inst: toErase) {
            list.erase(inst);
        }
    }
    return result;
}

void SCCCallGraph::validate() const {
#if SC_DEBUG
    return;
    for (auto& function: *mod) {
        /// Gather all call instructions of the current function in a hash table
        auto const callInstructions = function.instructions() |
                                      ranges::views::filter([](auto& inst) {
            auto* call = dyncast<Call const*>(&inst);
            return call && isa<Function>(call->function());
        }) | ranges::views::transform([](auto& inst) -> auto* {
            return cast<Call const*>(&inst);
        }) | ranges::to<utl::hashset<Call const*>>;
        auto& node = (*this)[&function];
        /// We check that all necessary edges are present in the current
        /// representation of the call graph
        if (false) {
            for (auto* call: callInstructions) {
                auto* callee = cast<Function const*>(call->function());
                auto& calleeNode = (*this)[callee];
                auto callsites = node.callsites(calleeNode);
                SC_ASSERT(ranges::contains(callsites, call),
                          "This `call` instruction in the function is not "
                          "represented by the call graph");
            }
        }
        /// We check that all edges in the current representation of the call
        /// graph correspond to actual calls
        for (auto* calleeNode: node.callees()) {
            for (auto* call: node.callsites(*calleeNode)) {
                SC_ASSERT(callInstructions.contains(call),
                          "This `call` instruction in the call graph is not "
                          "present in the function");
            }
        }
    }
#endif // SC_DEBUG
}

void SCCCallGraph::updateFunctionPointer(FunctionNode* node,
                                         ir::Function* newFunction) {
    auto* oldFunction = &node->function();
    node->setPayload(newFunction);
    auto itr = _funcMap.find(oldFunction);
    SC_ASSERT(itr != _funcMap.end(), "Not found");
    _funcMap.erase(itr);
    _funcMap[newFunction] = node;
}

void opt::generateGraphviz(SCCCallGraph const& graph, std::ostream& ostream) {
    using namespace graphgen;
    auto* G = Graph::make(ID(0));
    utl::hashset<std::pair<ID, ID>> edges;
    for (auto& scc: graph.sccs()) {
        auto* sccGraph = Graph::make(ID(&scc));
        G->add(sccGraph);
        for (auto* node: scc.nodes()) {
            auto* vertex = Vertex::make(ID(node))->label(
                std::string(node->function().name()));
            sccGraph->add(vertex);
            for (auto* callee: node->callees()) {
                Edge edge{ ID(node), ID(callee) };
                if (edges.insert({ edge.from, edge.to }).second) {
                    G->add(edge);
                }
            }
        }
    }
    Graph H;
    H.label("Callgraph");
    H.kind(graphgen::GraphKind::Directed);
    H.add(G);
    H.font("SF Mono");
    graphgen::generate(H, ostream);
}

void opt::generateGraphvizTmp(SCCCallGraph const& graph) {
    try {
        auto [path, file] = debug::newDebugFile("Callgraph");
        generateGraphviz(graph, file);
        file.close();
        debug::createGraphAndOpen(path);
    }
    catch (std::exception const& e) {
        std::cout << e.what() << std::endl;
    }
}
