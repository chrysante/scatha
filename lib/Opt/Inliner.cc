#include "Opt/Inliner.h"

#include "IR/CFG.h"
#include "Opt/CallGraph.h"
#include "Opt/InlineCallsite.h"

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
    /// Most naive heuristic ever: Inline if we have less than 7 instructions.
    if (ranges::distance(callee.function().instructions()) < 7) {
        return true;
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

    bool processSCC(SCC const& scc);

    bool processFunction(FunctionNode const& node);

    utl::small_vector<SCC const*> gatherLeaves() const;

    bool allSuccessorsAnalyzed(SCC const& scc) const;

    Context& ctx;
    Module& mod;
    SCCCallGraph callGraph;
    utl::hashset<SCC const*> analyzed;
};

} // namespace

bool opt::inlineFunctions(ir::Context& ctx, Module& mod) {
    Inliner inl(ctx, mod);
    inl.run();
    return false;
}

bool Inliner::run() {
    auto worklist    = gatherLeaves() | ranges::to<utl::hashset<SCC const*>>;
    bool modifiedAny = false;
    while (!worklist.empty()) {
        auto itr = worklist.begin();
        while (!allSuccessorsAnalyzed(**itr)) {
            ++itr;
        }
        SCC const& scc = **itr;
        worklist.erase(itr);
        modifiedAny |= processSCC(scc);
        analyzed.insert(&scc);
        for (auto& pred: scc.predecessors()) {
            worklist.insert(&pred);
        }
    }
    return modifiedAny;
}

bool Inliner::processSCC(SCC const& scc) {
    utl::hashset<FunctionNode const*> visited;
    bool modifiedAny = false;
    auto walk        = [&](FunctionNode const& node, auto& walk) {
        if (!visited.insert(&node).second) {
            return;
        }
        modifiedAny |= processFunction(node);
        for (auto& pred: node.predecessors()) {
            if (&pred.scc() == &scc) {
                walk(pred, walk);
            }
        }
    };
    walk(scc.nodes().front(), walk);
    return modifiedAny;
}

bool Inliner::processFunction(FunctionNode const& node) {
    for (auto& callee: node.callees()) {
        auto callsitesOfCallee = node.callsites(callee);
        for (auto* callInst: callsitesOfCallee) {
            bool const shouldInline = shouldInlineCallsite(callGraph, callInst);
            if (shouldInline) {
                inlineCallsite(ctx, callInst);
            }
        }
    }
    return false;
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
