#include "Opt/Passes.h"

#include <utl/hashtable.hpp>
#include <utl/vector.hpp>

#include "IR/CFG.h"
#include "IR/Module.h"
#include "Opt/PassRegistry.h"
#include "Opt/SCCCallGraph.h"

using namespace scatha;
using namespace opt;
using namespace ir;

SC_REGISTER_GLOBAL_PASS(opt::deadFuncElim, "deadfuncelim");

using Node = SCCCallGraph::FunctionNode;

namespace {

struct DFEContext {
    DFEContext(Context& ctx, Module& mod):
        ctx(ctx), mod(mod), callgraph(SCCCallGraph::computeNoSCCs(mod)) {}

    bool run();

    void visit(Function* node);

    Context& ctx;
    Module& mod;
    SCCCallGraph callgraph;
    utl::hashset<Function*> live;
};

} // namespace

bool opt::deadFuncElim(Context& ctx, Module& mod) {
    return DFEContext(ctx, mod).run();
}

bool opt::deadFuncElim(Context& ctx, Module& mod, LocalPass) {
    return deadFuncElim(ctx, mod);
}

bool DFEContext::run() {
    for (auto& F: mod) {
        if (F.visibility() == Visibility::Extern) {
            visit(&F);
        }
    }
    bool modified = false;
    for (auto itr = mod.begin(); itr != mod.end();) {
        auto* F = itr.to_address();
        if (live.contains(F)) {
            ++itr;
            continue;
        }
        itr = mod.eraseFunction(itr);
    }
    return modified;
}

void DFEContext::visit(Function* F) {
    if (live.contains(F)) {
        return;
    }
    live.insert(F);
    auto& node = callgraph[F];
    for (auto* calleeNode: node.callees()) {
        visit(&calleeNode->function());
    }
}
