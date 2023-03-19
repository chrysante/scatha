#include "Opt/CallGraph.h"

#include "IR/Module.h"
#include "IR/CFG.h"

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
