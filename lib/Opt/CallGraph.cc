#include "Opt/CallGraph.h"

#include <utl/stack.hpp>

#include "IR/CFG.h"
#include "IR/Module.h"

using namespace scatha;
using namespace ir;
using namespace opt;

CallGraph CallGraph::build(Module& mod) {
    CallGraph result;
    result._nodes = mod.functions() |
                    ranges::views::transform([](Function& function) {
                        return CallGraph::Node(&function);
                    }) |
                    ranges::to<NodeSet>;
    for (auto& function: mod.functions()) {
        for (auto& inst: function.instructions()) {
            auto* call = dyncast<FunctionCall*>(&inst);
            if (!call) {
                continue;
            }
            auto& thisNode = result.findMut(&function);
            auto& succNode = result.findMut(call->function());
            thisNode.addSuccessor(&succNode);
            succNode.addPredecessor(&thisNode);
        }
    }
    return result;
}

namespace {

struct VertexData {
    uint32_t index:   31 = 0;
    bool defined:      1 = false;
    uint32_t lowlink: 31 = 0;
    bool     onStack:  1 = false;
};

struct SCCComputeContext {
    SCCComputeContext(CallGraph const& callGraph,
                      utl::vector<utl::small_vector<ir::Function*>>& result):
        callGraph(callGraph), result(result) {}
    
    void compute();
    
    void strongConnect(CallGraph::Node const& node);
    
    CallGraph const& callGraph;
    utl::vector<utl::small_vector<ir::Function*>>& result;
    utl::stack<CallGraph::Node const*> stack;
    uint32_t index = 0;
    utl::hashmap<CallGraph::Node const*, VertexData> vertexData;
};

} // namespace

utl::vector<utl::small_vector<ir::Function*>>
opt::computeSCCs(CallGraph const& callGraph) {
    utl::vector<utl::small_vector<ir::Function*>> result;
    SCCComputeContext ctx(callGraph, result);
    ctx.compute();
    return result;
}

void SCCComputeContext::compute() {
    vertexData = callGraph.nodes() |
                 ranges::views::transform([](CallGraph::Node const& node) {
                     return std::pair{ &node, VertexData{} };
                 }) |
                 ranges::to<utl::hashmap<CallGraph::Node const*, VertexData>>;
    for (auto& node: callGraph.nodes()) {
        if (!vertexData[&node].defined) {
            strongConnect(node);
        }
    }
}

void SCCComputeContext::strongConnect(CallGraph::Node const& v) {
    auto& vData = vertexData[&v];
    /// Set the depth index for `v` to the smallest unused index
    vData.index = index;
    vData.defined = true;
    vData.lowlink = index;
    ++index;
    stack.push(&v);
    vData.onStack = true;
    
    for (auto& w: v.successors()) {
        auto& wData = vertexData[&w];
        if (!wData.defined) {
            //// Successor `w` has not yet been visited; recurse on it.
            strongConnect(w);
            vData.lowlink = std::min(vData.lowlink, wData.lowlink);
        }
        else if (wData.onStack) {
            /// Successor `w` is in stack S and hence in the current SCC.
            /// If `w` is not on stack, then `(v, w)` is an edge pointing to an SCC already found and must be ignored.
            /// Note: The next line may look odd - but is correct.
            /// It says `wData.index`, not `wData.lowLink`; that is deliberate and from the original paper.
            vData.lowlink = std::min(vData.lowlink, wData.index);
        }
    }
    /// If `v` is a root node, pop the stack and generate an SCC.
    if (vData.lowlink == vData.index) {
        utl::small_vector<ir::Function*> component;
        CallGraph::Node const* wPtr = nullptr;
        do {
            wPtr = stack.pop();
            auto& w = *wPtr;
            auto& wData = vertexData[&w];
            wData.onStack = false;
            component.push_back(w.function());
        } while (wPtr != &v);
        result.push_back(std::move(component));
    }
}
