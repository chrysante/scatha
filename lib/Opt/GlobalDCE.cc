#include "Opt/Passes.h"

#include <utl/hashtable.hpp>
#include <utl/vector.hpp>

#include "IR/CFG.h"
#include "IR/Module.h"
#include "IR/PassRegistry.h"
#include "Opt/SCCCallGraph.h"

using namespace scatha;
using namespace opt;
using namespace ir;

/// Expose the pass to the pass manager. Therefore we need a function that
/// accepts a local pass as the third argument
static bool globalDCEPass(Context& ctx, Module& mod, LocalPass) {
    return globalDCE(ctx, mod);
}

SC_REGISTER_GLOBAL_PASS(globalDCEPass, "globaldce");

using Node = SCCCallGraph::FunctionNode;

namespace {

struct GDCEContext {
    GDCEContext(Context& ctx, Module& mod):
        ctx(ctx), mod(mod), callgraph(SCCCallGraph::computeNoSCCs(mod)) {}

    bool run();

    /// Recursively inserts callees into the `live` set
    void visit(SCCCallGraph::FunctionNode const* node);

    Context& ctx;
    Module& mod;
    SCCCallGraph callgraph;
    utl::hashset<Function*> live;
};

} // namespace

bool opt::globalDCE(Context& ctx, Module& mod) {
    return GDCEContext(ctx, mod).run();
}

bool GDCEContext::run() {
    /// Determine all live functions
    for (auto& F: mod) {
        if (F.visibility() == Visibility::External) {
            visit(&callgraph[&F]);
        }
    }
    /// Erase all live functions
    bool modified = false;
    for (auto itr = mod.begin(); itr != mod.end();) {
        auto* F = itr.to_address();
        if (live.contains(F)) {
            ++itr;
            continue;
        }
        itr = mod.erase(itr);
        modified = true;
    }
    utl::small_vector<Global*> unused;
    for (auto& global: mod.globals()) {
        if (global.unused()) {
            unused.push_back(&global);
        }
    }
    for (auto* global: unused) {
        mod.erase(global);
    }
    return modified;
}

void GDCEContext::visit(SCCCallGraph::FunctionNode const* node) {
    if (!live.insert(&node->function()).second) {
        return;
    }
    for (auto* succ: node->callees()) {
        visit(succ);
    }
}
