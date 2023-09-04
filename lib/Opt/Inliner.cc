#include "Opt/Passes.h"

#include "Common/Ranges.h"
#include "IR/CFG.h"
#include "IR/Clone.h"
#include "IR/Module.h"
#include "IR/Validate.h"
#include "Opt/Common.h"
#include "Opt/InlineCallsite.h"
#include "Opt/PassRegistry.h"
#include "Opt/Passes.h"
#include "Opt/SCCCallGraph.h"

using namespace scatha;
using namespace ir;
using namespace opt;

SC_REGISTER_GLOBAL_PASS(opt::inlineFunctions, "inline");

using SCC = SCCCallGraph::SCCNode;
using FunctionNode = SCCCallGraph::FunctionNode;
using RemoveCallEdgeResult = SCCCallGraph::RemoveCallEdgeResult;

static bool shouldInlineCallsite(SCCCallGraph const& callGraph,
                                 Call const* call) {
    SC_ASSERT(isa<Function>(call->function()), "");
    auto& caller = callGraph[call->parentFunction()];
    auto& callee = callGraph[cast<Function const*>(call->function())];
    /// Here we ignore inline direct recursion.
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
    explicit Inliner(Context& ctx, Module& mod, LocalPass localPass):
        ctx(ctx),
        mod(mod),
        localPass(std::move(localPass)),
        callGraph(SCCCallGraph::compute(mod)) {}

    bool run();

    /// Called for every SCC whose successors have been fully optimized
    VisitResult visitSCC(SCC& scc);

    /// After analyzing an SCC, this function is called for every function of
    /// the SCC. If it fails to eliminate self recursion,  it adds the function
    /// to the `selfRecursive` set.
    bool inlineSelfRecursion(ir::Function* function);

    /// \returns `true` if the self recursion has been eliminated
    bool inlineSelfRecImpl(ir::Function* clone,
                           ir::Function* function,
                           int numLayers);

    /// Called for every function in an SCC
    VisitResult visitFunction(FunctionNode const& node);

    /// Inline a call.
    /// If the callee is self recursive, inlining is only attempted if some
    /// arguments are constant. In this case we only inline if thereby we can
    /// eliminate all recursive calls.
    bool doInline(Call* callInst);

    /// Collects all sinks of the quotient call graph
    utl::small_vector<SCC*> gatherSinks();

    bool allSuccessorsAnalyzed(SCC const& scc) const;

    /// Performs all local optimization passes on a function.
    bool optimize(Function& function) const;

    Context& ctx;
    Module& mod;
    LocalPass localPass;
    SCCCallGraph callGraph;
    utl::hashset<SCC const*> analyzed;
    utl::hashset<Function const*> selfRecursive;
};

} // namespace

bool opt::inlineFunctions(ir::Context& ctx, Module& mod) {
    return inlineFunctions(ctx, mod, opt::defaultPass);
}

bool opt::inlineFunctions(ir::Context& ctx,
                          ir::Module& mod,
                          LocalPass localPass) {
    Inliner inl(ctx, mod, std::move(localPass));
    return inl.run();
}

bool Inliner::run() {
    auto worklist = gatherSinks() | ranges::to<utl::hashset<SCC*>>;
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
    bool modifiedAny = false;
    /// Perform one local optimization pass on every function before traversing
    /// the SCC. Otherwise, because we are in a cyclic component,  there will
    /// always be a function which will not have been optimized before being
    /// considered for inlining.
    /// This is the first time any optimization is run on the function so here
    /// we canonicalize
    for (auto& node: scc.nodes()) {
        modifiedAny |= canonicalize(ctx, node.function());
        modifiedAny |= optimize(node.function());
        /// We recompute the call sites after local optimizations because they
        /// could have been invalidated
        node.recomputeCallees(callGraph);
    }
    utl::hashset<FunctionNode const*> visited;
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
    /// Here we have fully optimized the SCC
    /// We will now try to inline self recursive functions
    /// As of right now this can be crazy slow for big recursive functions
    /// because it does inline unconditionally and thus creates humongous
    /// functions that are mostly discarded if the self recursion can't be
    /// eliminated. That's why it's disabled.
#if 0
    for (auto& node: scc.nodes()) {
        modifiedAny |= inlineSelfRecursion(&node.function());
    }
#endif
    return modifiedAny;
}

VisitResult Inliner::visitFunction(FunctionNode const& node) {
    /// We have already locally optimized this function. Now we try to inline
    /// callees.
    bool modifiedAny = false;
    /// We create a copy of the list of callees because after inlining one
    /// function the corresponding edge may be erased from the call graph,
    /// invalidating the list.
    auto callees = node.callees() | ToSmallVector<>;
    for (auto* callee: callees) {
        auto callsitesOfCallee = node.callsites(*callee);
        for (auto* callInst: callsitesOfCallee) {
            bool const shouldInline = shouldInlineCallsite(callGraph, callInst);
            if (!shouldInline) {
                continue;
            }
            if (!doInline(callInst)) {
                continue;
            }
            modifiedAny = true;
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
        ir::assertInvariants(ctx, node.function());
    }
    return modifiedAny;
}

static bool isConstant(Value const* value) {
    if (isa<Constant>(value)) {
        return true;
    }
    while (true) {
        auto* iv = dyncast<InsertValue const*>(value);
        if (!iv) {
            return false;
        }
        if (isa<Constant>(iv->insertedValue())) {
            return true;
        }
        if (isa<Constant>(iv->baseValue()) && !isa<UndefValue>(iv->baseValue()))
        {
            return true;
        }
        value = iv->baseValue();
    }
    return false;
}

static utl::small_vector<uint16_t> constantArgIndices(Call const* callInst) {
    utl::small_vector<uint16_t> result;
    for (auto [index, arg]: callInst->arguments() | ranges::views::enumerate) {
        if (isConstant(arg)) {
            result.push_back(utl::narrow_cast<uint16_t>(index));
        }
    }
    return result;
}

static bool callsFunction(ir::Function* caller, ir::Function* callee) {
    for (auto& call: caller->instructions() | Filter<Call>) {
        if (call.function() == callee) {
            return true;
        }
    }
    return false;
}

static utl::small_vector<ir::Call*> gatherCallsTo(ir::Function* caller,
                                                  ir::Function* callee) {
    utl::small_vector<Call*> result;
    for (auto& call: caller->instructions() | Filter<Call>) {
        if (call.function() != callee) {
            continue;
        }
        result.push_back(&call);
    }
    return result;
}

bool Inliner::doInline(Call* callInst) {
    auto* callee = cast<Function*>(callInst->function());
    if (!selfRecursive.contains(callee)) {
        inlineCallsite(ctx, callInst);
        return true;
    }
    /// Now we try to inline a self recursive function into the caller _if_ we
    /// have constant arguments and can eliminate the recursion.
    auto const indices = constantArgIndices(callInst);
    if (indices.empty()) {
        return false;
    }
    auto clone = ir::clone(ctx, callee);
    for (size_t index: indices) {
        auto* param = std::next(clone->parameters().begin(),
                                utl::narrow_cast<ssize_t>(index))
                          .to_address();
        auto* arg = callInst->argumentAt(index);
        replaceValue(param, arg);
    }
    if (!inlineSelfRecImpl(clone.get(),
                           callee,
                           /* numLayers = */ 10))
    {
        return false;
    }

    inlineCallsite(ctx, callInst, std::move(clone));
    return true;
}

bool Inliner::inlineSelfRecursion(ir::Function* function) {
    if (!callsFunction(function, function)) {
        return false;
    }
    auto clone = ir::clone(ctx, function);
    if (!inlineSelfRecImpl(clone.get(), function, /* numLayers = */ 3)) {
        selfRecursive.insert(function);
        return false;
    }
    replaceValue(function, clone.get());
    auto& node = callGraph[function];
    callGraph.updateFunctionPointer(&node, clone.get());
    mod.eraseFunction(function);
    mod.addFunction(std::move(clone));
    return true;
}

bool Inliner::inlineSelfRecImpl(ir::Function* clone,
                                ir::Function* function,
                                int numLayers) {
    SC_ASSERT(callsFunction(clone, function),
              "Function must be self recursive");
    size_t numCallsInlined = 0;
    size_t const maxCallsInlined = 30;
    for (int i = 0; i < numLayers; ++i) {
        utl::small_vector<Call*> callsToSelf = gatherCallsTo(clone, function);
        if (callsToSelf.empty()) {
            break;
        }
        for (auto* call: callsToSelf) {
            inlineCallsite(ctx, call);
            /// We have to limit the number of calls that we inline, because
            /// functions like the ackermann function would otherwise be inlined
            /// forever.
            ++numCallsInlined;
            if (numCallsInlined > maxCallsInlined) {
                return false;
            }
        }
        optimize(*clone);
    }
    /// If we are still self recursive after inlining `numLayers` layers of
    /// recursion, we conclude the recursion can't be optimized away.
    return !callsFunction(clone, function);
}

utl::small_vector<SCC*> Inliner::gatherSinks() {
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
    bool modifiedAny = false;
    int const tripLimit = 4;
    for (int i = 0; i < tripLimit; ++i) {
        if (!localPass(ctx, function)) {
            break;
        }
        modifiedAny = true;
    }
    return modifiedAny;
}
