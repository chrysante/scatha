#include "Opt/Passes.h"

#include <utl/hashtable.hpp>
#include <utl/vector.hpp>

#include "IR/CFG.h"
#include "IR/Context.h"
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

SC_REGISTER_GLOBAL_PASS(globalDCEPass, "globaldce",
                        PassCategory::Simplification);

using Node = SCCCallGraph::FunctionNode;

namespace {

struct GDCEContext {
    GDCEContext(Context& ctx, Module& mod):
        ctx(ctx), mod(mod), callgraph(SCCCallGraph::computeNoSCCs(mod)) {}

    bool run();

    bool eraseUnusedGlobals();
    bool eraseUnusedFunctions();
    /// Recursively inserts callees into the `live` set
    void visit(SCCCallGraph::FunctionNode const* node,
               utl::hashset<Function*>& live);

    Context& ctx;
    Module& mod;
    SCCCallGraph callgraph;
};

} // namespace

bool opt::globalDCE(Context& ctx, Module& mod) {
    return GDCEContext(ctx, mod).run();
}

bool GDCEContext::eraseUnusedGlobals() {
    utl::small_vector<Global*> unused;
    for (auto& global: mod.globals()) {
        if (global.visibility() != Visibility::External && global.unused()) {
            unused.push_back(&global);
        }
    }
    for (auto* global: unused) {
        mod.erase(global);
    }
    return !unused.empty();
}

bool GDCEContext::eraseUnusedFunctions() {
    utl::hashset<Function*> live;
    for (auto& F: mod) {
        bool isReferenced = ranges::any_of(F.users(), [](auto* user) {
            return !isa<Call>(user);
        });
        if (isReferenced || F.visibility() == Visibility::External) {
            visit(callgraph[&F], live);
        }
        if (isReferenced) {
            live.insert(&F);
        }
    }
    bool modified = false;
    for (auto itr = mod.begin(); itr != mod.end();) {
        auto* F = itr.to_address();
        ++itr;
        if (!live.contains(F)) {
            mod.erase(F);
            modified = true;
        }
    }
    return modified;
}

bool GDCEContext::run() {
    bool modified = false;
    while (true) {
        bool modThisIter = false;
        modThisIter |= eraseUnusedGlobals();
        modThisIter |= eraseUnusedFunctions();
        modThisIter |= ctx.cleanConstants();
        if (!modThisIter) {
            break;
        }
        modified = modThisIter;
    }
    return modified;
}

void GDCEContext::visit(SCCCallGraph::FunctionNode const* node,
                        utl::hashset<Function*>& live) {
    if (!live.insert(node->function()).second) {
        return;
    }
    for (auto* succ: node->callees()) {
        visit(succ, live);
    }
}
