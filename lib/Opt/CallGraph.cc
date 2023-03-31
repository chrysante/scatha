#include "Opt/CallGraph.h"

#include <utl/stack.hpp>

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

void SCCCallGraph::computeCallGraph(Module& mod) {
    _functions = mod |
                 ranges::views::transform([](Function& function) {
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

namespace {

struct VertexData {
    uint32_t index   : 31 = 0;
    bool defined     : 1  = false;
    uint32_t lowlink : 31 = 0;
    bool onStack     : 1  = false;
};

} // namespace

void SCCCallGraph::computeSCCs() {
    /// Algorithm from here:
    /// https://en.wikipedia.org/wiki/Tarjan%27s_strongly_connected_components_algorithm
    struct TSCCAContext {
        TSCCAContext(SCCCallGraph& SCCCallGraph): callGraph(SCCCallGraph) {}

        void compute() {
            vertexData =
                callGraph._functions |
                ranges::views::transform([](FunctionNode const& node) {
                    return std::pair{ &node, VertexData{} };
                }) |
                ranges::to<utl::hashmap<FunctionNode const*, VertexData>>;
            for (auto& node: callGraph._functions) {
                if (!vertexData[&node].defined) {
                    strongConnect(node);
                }
            }
        }

        void strongConnect(FunctionNode const& v) {
            auto& vData = vertexData[&v];
            /// Set the depth index for `v` to the smallest unused index
            vData.index   = index;
            vData.defined = true;
            vData.lowlink = index;
            ++index;
            stack.push(&v);
            vData.onStack = true;
            for (auto* w: v.successors()) {
                auto& wData = vertexData[w];
                if (!wData.defined) {
                    //// Successor `w` has not yet been visited; recurse on it.
                    strongConnect(*w);
                    vData.lowlink = std::min(vData.lowlink, wData.lowlink);
                }
                else if (wData.onStack) {
                    /// Successor `w` is in `stack` and hence in the current
                    /// SCC. If `w` is not on stack, then `(v, w)` is an edge
                    /// pointing to an SCC already found and must be ignored.
                    /// Note: The next line may look odd - but is correct. It
                    /// says `wData.index`, not `wData.lowlink`; that is
                    /// deliberate and from the original paper.
                    vData.lowlink = std::min(vData.lowlink, wData.index);
                }
            }
            /// If `v` is a root node, pop the stack and generate an SCC.
            if (vData.lowlink == vData.index) {
                SCCNode component;
                FunctionNode const* wPtr = nullptr;
                do {
                    wPtr          = stack.pop();
                    auto& w       = *wPtr;
                    auto& wData   = vertexData[&w];
                    wData.onStack = false;
                    /// `const_cast` looks suspicious but is fine here. We have
                    /// a const reference because `node` is in a set, but only
                    /// keyed on its `Function` pointer. So as long as wo don't
                    /// change that field we are fine.
                    component._nodes.push_back(const_cast<FunctionNode*>(wPtr));
                } while (wPtr != &v);
                callGraph._sccs.push_back(std::move(component));
            }
        }

        SCCCallGraph& callGraph;
        utl::stack<FunctionNode const*> stack;
        uint32_t index = 0;
        utl::hashmap<FunctionNode const*, VertexData> vertexData;
    };

    TSCCAContext ctx(*this);
    ctx.compute();

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
