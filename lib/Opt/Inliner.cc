#include "Opt/Inliner.h"

#include "IR/CFG.h"
#include "Opt/CallGraph.h"
#include "Opt/ConstantPropagation.h"
#include "Opt/InlineCallsite.h"
#include "Opt/MemToReg.h"
#include "Opt/SimplifyCFG.h"

using namespace scatha;
using namespace ir;
using namespace opt;

using SCC = SCCCallGraph::SCCNode;

using FunctionNode = SCCCallGraph::FunctionNode;

static bool shouldInlineCallsite(SCCCallGraph const& callGraph,
                                 FunctionCall const* call) {
    auto& caller = callGraph[call->parent()->parent()];
    auto& callee = callGraph[call->function()];
    /// We can inline direct recursion, not yet at least...
    if (&caller == &callee) {
        return false;
    }
    auto const calleeNumInstructions =
        ranges::distance(callee.function().instructions());
    /// Most naive heuristic ever: Inline if we have less than 14 instructions.
    if (calleeNumInstructions < 14) {
        return true;
    }
    /// If we have constant arguments, then there are more opportunities for
    /// optimization, so we inline more aggressively.
    if (ranges::any_of(call->arguments(),
                       [](Value const* value) { return isa<Constant>(value); }))
    {
        if (calleeNumInstructions < 21) {
            return true;
        }
    }
    /// Also always inline if we are the only user of this function.
    if (callee.function().users().size() <= 1) {
        return true;
    }
    return false;
}

namespace {

struct Inliner {
    explicit Inliner(Context& ctx, Module& mod):
        ctx(ctx), mod(mod), callGraph(SCCCallGraph::compute(mod)) {}

    bool run();

    bool visitSCC(SCC const& scc);

    bool visitFunction(FunctionNode const& node);

    utl::small_vector<SCC const*> gatherLeaves() const;

    bool allSuccessorsAnalyzed(SCC const& scc) const;

    bool optimize(Function& function) const;

    Context& ctx;
    Module& mod;
    SCCCallGraph callGraph;
    utl::hashset<SCC const*> analyzed;
};

} // namespace

bool opt::inlineFunctions(ir::Context& ctx, Module& mod) {
    Inliner inl(ctx, mod);
    return inl.run();
}

bool Inliner::run() {
    auto worklist    = gatherLeaves() | ranges::to<utl::hashset<SCC const*>>;
    bool modifiedAny = false;
    while (!worklist.empty()) {
        auto itr = worklist.begin();
        while (!allSuccessorsAnalyzed(**itr)) {
            ++itr;
            SC_ASSERT(itr != worklist.end(),
                      "We have no component in the worklist that has all "
                      "successors analyzed.");
        }
        SCC const& scc = **itr;
        worklist.erase(itr);
        modifiedAny |= visitSCC(scc);
        analyzed.insert(&scc);
        for (auto& pred: scc.predecessors()) {
            worklist.insert(&pred);
        }
    }
    return modifiedAny;
}

bool Inliner::visitSCC(SCC const& scc) {
    /// Perform one local optimization pass on every function before traversing
    /// the SCC. Otherwise, because we are in a cyclic component,  there will
    /// always be a function which will not have been optimized before being
    /// considered for inlining.
    for (auto& node: scc.nodes()) {
        optimize(node.function());
    }
    utl::hashset<FunctionNode const*> visited;
    bool modifiedAny = false;
    auto walk        = [&](FunctionNode const& node, auto& walk) {
        if (!visited.insert(&node).second) {
            return;
        }
        modifiedAny |= visitFunction(node);
        for (auto& pred: node.predecessors()) {
            if (&pred.scc() == &scc) {
                walk(pred, walk);
            }
        }
    };
    walk(scc.nodes().front(), walk);
    return modifiedAny;
}

bool Inliner::visitFunction(FunctionNode const& node) {
    /// We optimize this function, then inline callees, then optimize again to
    /// catch optimization opportunities emerged from inlining.
    bool modifiedAny = false;
    modifiedAny |= optimize(node.function());
    for (auto& callee: node.callees()) {
        auto callsitesOfCallee = node.callsites(callee);
        for (auto* callInst: callsitesOfCallee) {
            bool const shouldInline = shouldInlineCallsite(callGraph, callInst);
            if (shouldInline) {
                inlineCallsite(ctx, callInst);
                modifiedAny = true;
            }
        }
    }
    modifiedAny |= optimize(node.function());
    return modifiedAny;
}

utl::small_vector<SCC const*> Inliner::gatherLeaves() const {
    utl::small_vector<SCC const*> result;
    for (auto& scc: callGraph.sccs()) {
        if (scc.successors().empty()) {
            result.push_back(&scc);
        }
    }
    return result;
}

bool Inliner::allSuccessorsAnalyzed(SCC const& scc) const {
    for (auto& succ: scc.successors()) {
        if (!analyzed.contains(&succ)) {
            return false;
        }
    }
    return true;
}

bool Inliner::optimize(Function& function) const {
    bool modifiedAny = false;
    modifiedAny |= memToReg(ctx, function);
    modifiedAny |= propagateConstants(ctx, function);
    modifiedAny |= simplifyCFG(ctx, function);
    return modifiedAny;
}
