#include "Opt/SCCCallGraph.h"

#include <utl/graph.hpp>

#include "IR/CFG.h"
#include "IR/Module.h"

using namespace scatha;
using namespace ir;
using namespace opt;

std::span<ir::Call* const> SCCCallGraph::FunctionNode::callsites(
    FunctionNode const& callee) const {
    auto itr = _callsites.find(&callee);
    SC_ASSERT(itr != _callsites.end(), "Not found");
    return itr->second;
}

SCCCallGraph SCCCallGraph::compute(Module& mod) {
    SCCCallGraph result;
    result.computeCallGraph(mod);
    result.computeSCCs();
    return result;
}

SCCCallGraph SCCCallGraph::computeNoSCCs(Module& mod) {
    SCCCallGraph result;
    result.computeCallGraph(mod);
    return result;
}

void SCCCallGraph::computeCallGraph(Module& mod) {
    _functions = mod | ranges::views::transform([](Function& function) {
                     return FunctionNode(&function);
                 }) |
                 ranges::to<FuncNodeSet>;
    for (auto& function: mod) {
        for (auto& inst: function.instructions()) {
            auto* call = dyncast<Call*>(&inst);
            if (!call) {
                continue;
            }
            auto* target = dyncast<Function*>(call->function());
            if (!target) {
                continue;
            }
            /// We ignore self recursion.
            if (target == &function) {
                continue;
            }
            auto& thisNode = findMut(&function);
            auto& succNode = findMut(target);
            thisNode.addSuccessor(&succNode);
            succNode.addPredecessor(&thisNode);
            thisNode._callsites[&succNode].push_back(call);
        }
    }
}

void SCCCallGraph::computeSCCs() {
    auto vertices =
        _functions | ranges::views::transform([](auto& v) { return &v; });
    utl::compute_sccs(
        vertices.begin(),
        vertices.end(),
        [](FunctionNode const* node) { return node->successors(); },
        [&] { _sccs.emplace_back(); },
        [&](FunctionNode const* node) {
        auto& scc = _sccs.back();
        scc.addNode(const_cast<FunctionNode*>(node));
        });
    /// After we have computed the SCC's, we set up parent pointers of the
    /// function nodes.
    for (auto& scc: _sccs) {
        for (auto* function: scc._nodes) {
            function->_scc = &scc;
        }
    }
    /// Then we set up the remaining links to make the set of SCC's into a graph
    /// representing the call graph.
    for (auto& scc: _sccs) {
        for (auto& function: scc.nodes()) {
            for (auto* succ: function.successors()) {
                auto& succSCC = succ->scc();
                if (&succSCC == &scc) {
                    continue;
                }
                scc.addSuccessor(&succSCC);
                succSCC.addPredecessor(&scc);
            }
        }
    }
}
