#include "Opt/SCCCallGraph.h"

#include <utl/graph.hpp>

#include "Common/Ranges.h"
#include "IR/CFG.h"
#include "IR/Module.h"

using namespace scatha;
using namespace ir;
using namespace opt;

using FunctionNode = SCCCallGraph::FunctionNode;
using SCCNode = SCCCallGraph::SCCNode;

utl::hashset<ir::Call*> const& SCCCallGraph::FunctionNode::callsites(
    FunctionNode const& callee) const {
    auto itr = _callsites.find(&callee);
    SC_ASSERT(itr != _callsites.end(), "Not found");
    return itr->second;
}

static void removeOtherSuccessors(FunctionNode* node) {
    for (auto* succ: node->successors()) {
        succ->removePredecessor(node);
    }
}

void SCCCallGraph::FunctionNode::recomputeCallees(SCCCallGraph& callgraph) {
    removeOtherSuccessors(this);
    this->clearSuccessors();
    utl::hashmap<FunctionNode const*, utl::hashset<Call*>> newCallsites;
    for (auto& call: function().instructions() | Filter<Call>) {
        auto* callee = dyncast<Function*>(call.function());
        if (!callee) {
            continue;
        }
        auto* calleeNode = &callgraph.findMut(callee);
        newCallsites[calleeNode].insert(&call);
        addSuccessor(calleeNode);
        calleeNode->addPredecessor(this);
    }
    _callsites = newCallsites;
}

SCCCallGraph::SCCNode::SCCNode(utl::small_vector<FunctionNode*> nodes):
    _nodes(std::move(nodes)) {
    for (auto* node: _nodes) {
        node->_scc = this;
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
    }) | ToSmallVector<>;
    for (auto& node: _nodes) {
        _funcMap[&node->function()] = node.get();
    }
    for (auto& function: *mod) {
        for (auto& call: function.instructions() | Filter<Call>) {
            auto* target = dyncast<Function*>(call.function());
            if (!target) {
                continue;
            }
            /// We ignore self recursion, because the inliner handles it
            /// seperately
            if (target == &function) {
                continue;
            }
            auto& thisNode = findMut(&function);
            auto& succNode = findMut(target);
            thisNode.addSuccessor(&succNode);
            succNode.addPredecessor(&thisNode);
            thisNode._callsites[&succNode].insert(&call);
        }
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
            function->_scc = scc.get();
        }
    }
    /// Then we set up the remaining links to make the set of SCC's into a graph
    /// representing the call graph.
    for (auto& scc: _sccs) {
        for (auto& function: scc->nodes()) {
            for (auto* succ: function.successors()) {
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

    SCC_DFS(FunctionNode* from, FunctionNode* to):
        from(from), to(to), scc(&from->scc()) {}

    bool search() const { return searchImpl(from); }

    bool searchImpl(FunctionNode const* current) const {
        if (current == to) {
            return true;
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
    for (auto& predFn: pred->nodes()) {
        for (auto& succFn: succ->nodes()) {
            if (predFn.isSuccessor(&succFn)) {
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
        for (auto& predNode: predSCC->nodes()) {
            for (auto* succ: predNode.successors()) {
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

SCCCallGraph::RemoveCallEdgeResult SCCCallGraph::removeCall(
    Function* caller, Function const* callee, Call const* callInst) {
    auto& callerNode = findMut(caller);
    auto& calleeNode = findMut(callee);
    /// For calls devirtualized in local optimizations the call relation will
    /// not be reflected in the graph
    if (!callerNode.isPredecessor(&calleeNode)) {
        return RemoveCallEdgeResult::None;
    }
    /// We remove `call` from our list of call sites
    auto callsitesItr = callerNode._callsites.find(&calleeNode);
    SC_ASSERT(callsitesItr != callerNode._callsites.end(), "");
    auto& callsites = callsitesItr->second;
    auto callItr = callsites.find(callInst);
    SC_ASSERT(callItr != callsites.end(), "");
    callsites.erase(callItr);
    /// If there are still calls to `callee` left, the structure of the call
    /// graph is unchanged
    if (!callsites.empty()) {
        return RemoveCallEdgeResult::None;
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
        return RemoveCallEdgeResult::RemovedEdge;
    }
    /// Both nodes are in the same SCC. We perform two depth first searches
    /// _within the SCC_, to see if both nodes are still reachable from each
    /// other.
    if (SCC_DFS(&callerNode, &calleeNode).search() &&
        SCC_DFS(&calleeNode, &callerNode).search())
    {
        return RemoveCallEdgeResult::RemovedEdge;
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
    auto oldSCCItr =
        ranges::find_if(_sccs, [&](auto& scc) { return scc.get() == &oldSCC; });
    SC_ASSERT(oldSCCItr != _sccs.end(), "Old SCC not in list?");
    _sccs.erase(oldSCCItr);
    RemoveCallEdgeResult result = { RemoveCallEdgeResult::SplitSCC,
                                    callerSCC.get(), calleeSCC.get() };
    _sccs.push_back(std::move(callerSCC));
    _sccs.push_back(std::move(calleeSCC));
    return result;
}

void SCCCallGraph::validate() const {
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
        for (auto* call: callInstructions) {
            auto* callee = cast<Function const*>(call->function());
            auto& calleeNode = (*this)[callee];
            auto callsites = node.callsites(calleeNode);
            SC_RELASSERT(ranges::find(callsites, call) !=
                             ranges::end(callsites),
                         "This `call` instruction in the function is not "
                         "represented by the call graph");
        }
        /// We check that all edges in the current representation of the call
        /// graph correspond to actual calls
        for (auto* calleeNode: node.callees()) {
            for (auto* call: node.callsites(*calleeNode)) {
                SC_RELASSERT(callInstructions.contains(call),
                             "This `call` instruction in the call graph is not "
                             "present in the function");
            }
        }
    }
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
