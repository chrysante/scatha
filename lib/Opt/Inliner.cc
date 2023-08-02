#include "Opt/Inliner.h"

#include "IR/CFG.h"
#include "Opt/ConstantPropagation.h"
#include "Opt/DCE.h"
#include "Opt/InlineCallsite.h"
#include "Opt/InstCombine.h"
#include "Opt/LoopCanonical.h"
#include "Opt/MemToReg.h"
#include "Opt/SCCCallGraph.h"
#include "Opt/SROA.h"
#include "Opt/SimplifyCFG.h"
#include "Opt/TailRecElim.h"
#include "Opt/UnifyReturns.h"

using namespace scatha;
using namespace ir;
using namespace opt;

using SCC                  = SCCCallGraph::SCCNode;
using FunctionNode         = SCCCallGraph::FunctionNode;
using RemoveCallEdgeResult = SCCCallGraph::RemoveCallEdgeResult;

static bool shouldInlineCallsite(SCCCallGraph const& callGraph,
                                 Call const* call) {
    SC_ASSERT(isa<Function>(call->function()), "");
    auto& caller = callGraph[call->parentFunction()];
    auto& callee = callGraph[cast<Function const*>(call->function())];
    /// We can inline direct recursion, not yet at least...
    if (&caller == &callee) {
        return false;
    }
    auto const calleeNumInstructions =
        ranges::distance(callee.function().instructions());
    /// Most naive heuristic ever: Inline if we have less than 14 instructions.
    if (calleeNumInstructions < 40) {
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

struct VisitResult: RemoveCallEdgeResult {
    VisitResult(bool modified): modified(modified) {}

    VisitResult(bool modified, RemoveCallEdgeResult calledgeRemoval):
        RemoveCallEdgeResult(calledgeRemoval), modified(modified) {}

    bool modified;
};

struct Inliner {
    explicit Inliner(Context& ctx, Module& mod):
        ctx(ctx), mod(mod), callGraph(SCCCallGraph::compute(mod)) {}

    bool run();

    VisitResult visitSCC(SCC& scc);

    VisitResult visitFunction(FunctionNode const& node);

    utl::small_vector<SCC*> gatherLeaves();

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
    auto worklist    = gatherLeaves() | ranges::to<utl::hashset<SCC*>>;
    bool modifiedAny = false;
    while (!worklist.empty()) {
        auto itr = worklist.begin();
        while (!allSuccessorsAnalyzed(**itr)) {
            ++itr;
            SC_ASSERT(itr != worklist.end(),
                      "We have no component in the worklist that has all "
                      "successors analyzed.");
        }
        SCC& scc = **itr;
        worklist.erase(itr);
        auto result = visitSCC(scc);
        if (result.type == RemoveCallEdgeResult::SplitSCC) {
            worklist.insert({ result.newSCCs[0], result.newSCCs[1] });
            SC_ASSERT(result.modified,
                      "How can it not be modified if we split an SCC?");
            modifiedAny = true;
            continue;
        }
        modifiedAny |= result.modified;
        analyzed.insert(&scc);
        for (auto* pred: scc.predecessors()) {
            worklist.insert(pred);
        }
    }
    return modifiedAny;
}

VisitResult Inliner::visitSCC(SCC& scc) {
    /// Perform one local optimization pass on every function before traversing
    /// the SCC. Otherwise, because we are in a cyclic component,  there will
    /// always be a function which will not have been optimized before being
    /// considered for inlining.
    for (auto& node: scc.nodes()) {
        optimize(node.function());
        /// We recompute the call sites after local optimizations because they
        /// could have been invalidated
        node.recomputeCallees(callGraph);
    }
    utl::hashset<FunctionNode const*> visited;
    bool modifiedAny = false;
    /// Recursive function to walk the SCC.
    /// \returns `std::nullopt` on success.
    /// If an SCC is split during inlining, this function immediately returns a
    /// `RemoveCallEdgeResult` with `.type == SplitSCC` and the new SCCs
    auto walk = [&](FunctionNode const& node,
                    auto& walk) -> std::optional<RemoveCallEdgeResult> {
        if (!visited.insert(&node).second) {
            return std::nullopt;
        }
        auto result = visitFunction(node);
        modifiedAny |= result.modified;
        if (result.type == RemoveCallEdgeResult::SplitSCC) {
            return result;
        }
        for (auto* pred: node.predecessors()) {
            if (&pred->scc() == &scc) {
                if (auto result = walk(*pred, walk)) {
                    return result;
                }
            }
        }
        return std::nullopt;
    };
    if (auto result = walk(scc.nodes().front(), walk)) {
        SC_ASSERT(result->type == RemoveCallEdgeResult::SplitSCC,
                  "We only return a value from `walk` if we split an SCC");
        SC_ASSERT(modifiedAny, "");
        return { modifiedAny, *result };
    }
    return modifiedAny;
}

VisitResult Inliner::visitFunction(FunctionNode const& node) {
    /// We have already locally optimized this function. Now we try to inline
    /// callees.
    bool modifiedAny = false;
    for (auto* callee: node.callees()) {
        auto& callsitesOfCallee = node.callsites(*callee);
        for (auto* callInst: callsitesOfCallee) {
            bool const shouldInline = shouldInlineCallsite(callGraph, callInst);
            if (!shouldInline) {
                continue;
            }
            inlineCallsite(ctx, callInst);
            modifiedAny       = true;
            auto removeResult = callGraph.removeCall(&node.function(),
                                                     &callee->function(),
                                                     callInst);
            /// If the SCC has been split, we immediately return.
            /// Both new SCCs will be pushed to the worklist, so no inlining
            /// opportunities are missed.
            if (removeResult.type == RemoveCallEdgeResult::SplitSCC) {
                return { true, removeResult };
            }
        }
    }
    /// If we succeeded, we optimize again to catch optimization
    /// opportunities emerged from inlining.
    if (modifiedAny) {
        optimize(node.function());
    }
    return modifiedAny;
}

utl::small_vector<SCC*> Inliner::gatherLeaves() {
    utl::small_vector<SCC*> result;
    for (auto& scc: callGraph.sccs()) {
        if (scc.successors().empty()) {
            result.push_back(&scc);
        }
    }
    return result;
}

bool Inliner::allSuccessorsAnalyzed(SCC const& scc) const {
    for (auto* succ: scc.successors()) {
        if (!analyzed.contains(succ)) {
            return false;
        }
    }
    return true;
}

bool Inliner::optimize(Function& function) const {
    bool modifiedAny    = false;
    int const tripLimit = 4;
    for (int i = 0; i < tripLimit; ++i) {
        bool modified = false;
        modified |= sroa(ctx, function);
        modified |= memToReg(ctx, function);
        modified |= instCombine(ctx, function);
        modified |= propagateConstants(ctx, function);
        modified |= dce(ctx, function);
        modified |= simplifyCFG(ctx, function);
        modified |= tailRecElim(ctx, function);
        if (!modified) {
            break;
        }
        modifiedAny = true;
    }
    return modifiedAny;
}
