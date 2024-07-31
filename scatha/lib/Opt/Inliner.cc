#include "Opt/Passes.h"

#include <iostream>
#include <optional>

#include <range/v3/algorithm.hpp>
#include <range/v3/view.hpp>
#include <termfmt/termfmt.h>
#include <utl/scope_guard.hpp>

#include "Common/Logging.h"
#include "Common/Ranges.h"
#include "IR/CFG.h"
#include "IR/Clone.h"
#include "IR/Module.h"
#include "IR/PassRegistry.h"
#include "IR/Print.h"
#include "IR/Validate.h"
#include "Opt/Common.h"
#include "Opt/InlineCallsite.h"
#include "Opt/Passes.h"
#include "Opt/SCCCallGraph.h"

using namespace scatha;
using namespace ir;
using namespace opt;
using namespace ranges::views;

SC_REGISTER_MODULE_PASS(opt::inlineFunctions, "inline",
                        PassCategory::Simplification,
                        { Flag{ "print-vis-order", false },
                          Flag{ "print-decisions", false } });

namespace {

struct Arguments {
    static Arguments Parse(PassArgumentMap const& map) {
        return {
            .printVisOrder = map.get<bool>("print-vis-order"),
            .printDecisions = map.get<bool>("print-decisions"),
        };
    }

    bool printVisOrder = false;
    bool printDecisions = false;

    bool printAny() const { return printVisOrder || printDecisions; }
};

} // namespace

using SCC = SCCCallGraph::SCCNode;
using FunctionNode = SCCCallGraph::FunctionNode;
using Modification = SCCCallGraph::Modification;

namespace scatha::opt {

struct Inliner {
    explicit Inliner(Context& ctx, Module& mod, PassArgumentMap const& map,
                     FunctionPass functionPass):
        ctx(ctx),
        mod(mod),
        functionPass(std::move(functionPass)),
        callGraph(SCCCallGraph::compute(mod)),
        args(Arguments::Parse(map)) {}

    bool run();

    /// Called for every SCC whose successors have been fully optimized.
    /// \Returns
    /// - `true` if any function in the SCC has been modified
    /// - `false` if no functions have been modified
    /// - `std::nullopt` if structural changes have been made to the call graph
    /// and the SCC needs to be revisited later
    std::optional<bool> visitSCC(SCC& scc);

    /// After analyzing an SCC, this function is called for every function of
    /// the SCC
    void analyzeSelfRecursion(ir::Function* function);

    /// Called for every function in an SCC
    /// \Returns
    /// - `true` if the function has been modified
    /// - `false` if the function has not been modified
    /// - `std::nullopt` if structural changes have been made to the call graph
    /// and the function needs to be revisited later
    std::optional<bool> visitFunction(FunctionNode& node);

    /// Collects all sinks of the quotient call graph
    utl::small_vector<SCC*> gatherSinks();

    bool shouldInlineCallsite(Call const* call, int visitCount);

    bool allSuccessorsAnalyzed(SCC const& scc) const;

    /// Performs all local optimization passes on a function.
    bool optimize(Function& function) const;

    /// Inserts all modified SCCs into the worklist
    /// \Returns `true` if the currently visited function or SCC must be left
    /// and revisited
    bool handleCalleeRecompute(SCCCallGraph::RecomputeCalleesResult result);

    /// Debug utilities
    void printWorklist() const;

    ///
    void printVisit(Function const& F);

    ///
    void printDecision(Call const& inst, bool decision);

    Context& ctx;
    Module& mod;
    FunctionPass functionPass;
    SCCCallGraph callGraph;
    utl::hashset<SCC*> worklist;
    utl::hashset<SCC const*> analyzed;
    utl::hashmap<Function const*, int> visitCount;
    utl::hashset<Function const*> selfRecursive;
    utl::hashmap<Function const*, utl::hashset<Function const*>>
        incorporatedFunctions;
    Arguments args;
};

} // namespace scatha::opt

bool opt::inlineFunctions(ir::Context& ctx, Module& mod,
                          PassArgumentMap const& args) {
    return inlineFunctions(ctx, mod, opt::defaultPass, args);
}

bool opt::inlineFunctions(ir::Context& ctx, ir::Module& mod,
                          FunctionPass const& argPass,
                          PassArgumentMap const& args) {
    Inliner inl(ctx, mod, args, argPass ? argPass : defaultPass);
    return inl.run();
}

bool Inliner::run() {
    if (args.printAny()) {
        logging::subHeader("Inliner");
    }
    bool modifiedAny = false;
    worklist = gatherSinks() | ranges::to<utl::hashset<SCC*>>;
    while (!worklist.empty()) {
        auto itr = worklist.begin();
        while (!allSuccessorsAnalyzed(**itr)) {
            ++itr;
            SC_ASSERT(
                itr != worklist.end(),
                "We have no component in the worklist that has all successors analyzed.");
        }
        SCC& scc = **itr;
        auto visRes = visitSCC(scc);
        if (!visRes) {
            modifiedAny = true;
            continue;
        }
        worklist.erase(itr);
        modifiedAny |= *visRes;
        analyzed.insert(&scc);
        for (auto* pred: scc.predecessors()) {
            worklist.insert(pred);
        }
    }
    return modifiedAny;
}

std::optional<bool> Inliner::visitSCC(SCC& scc) {
    bool modifiedAny = false;
    /// Perform one local optimization pass on every function before traversing
    /// the SCC. Otherwise, because we are in a cyclic component,  there will
    /// always be a function which will not have been optimized before being
    /// considered for inlining.
    /// This is the first time any optimization is run on the function so here
    /// we canonicalize
    SCCCallGraph::RecomputeCalleesResult recomputeResult;
    for (auto* node: scc.nodes()) {
        modifiedAny |= canonicalize(ctx, *node->function());
        modifiedAny |= optimize(*node->function());
        /// We recompute the call sites after local optimizations because they
        /// could have been invalidated
        recomputeResult.merge(callGraph.recomputeCallees(*node));
    }
    if (handleCalleeRecompute(recomputeResult)) {
        return std::nullopt;
    }
    utl::hashset<FunctionNode const*> visited;
    /// Recursive function to walk the SCC.
    /// \returns `std::nullopt` on success.
    /// If an SCC is split during inlining, this function immediately returns a
    /// `RemoveCallEdgeResult` with `.type == SplitSCC` and the new SCCs
    auto dfs = [&](auto& dfs, FunctionNode* node) -> bool {
        if (!visited.insert(node).second) {
            return true;
        }
        auto result = visitFunction(*node);
        if (!result) {
            return false;
        }
        modifiedAny |= *result;
        for (auto* pred: node->predecessors()) {
            if (pred->scc() != &scc) {
                continue;
            }
            if (!dfs(dfs, pred)) {
                return false;
            }
        }
        return true;
    };
    if (!dfs(dfs, scc.nodes().front())) {
        return std::nullopt;
    }
    /// Here we have fully optimized the SCC
    /// We will now try to inline self recursive functions
    for (auto& node: scc.nodes()) {
        analyzeSelfRecursion(node->function());
    }
    return modifiedAny;
}

std::optional<bool> Inliner::visitFunction(FunctionNode& node) {
    printVisit(*node.function());
    auto& visitCount = this->visitCount[node.function()];
    utl::armed_scope_guard incGuard = [&] { ++visitCount; };
    /// We have already locally optimized this function. Now we try to inline
    /// callees.
    bool modifiedAny = false;
    utl::small_vector<Function const*> inlined;
    /// We create a copy of the list of callees because after inlining one
    /// function the corresponding edge may be erased from the call graph,
    /// invalidating the list.
    auto callees = node.callees() | ToSmallVector<>;
    for (auto* callee: callees) {
        auto callsitesOfCallee = node.callsites(callee);
        for (auto* callInst: callsitesOfCallee) {
            bool shouldInline = shouldInlineCallsite(callInst, visitCount);
            printDecision(*callInst, shouldInline);
            if (!shouldInline) {
                continue;
            }
            inlineCallsite(ctx, callInst);
            inlined.push_back(callee->function());
            modifiedAny = true;
            auto result = callGraph.removeCall(&node, callee, callInst);
            /// If the SCC has been split, we immediately return.
            /// Both new SCCs will be pushed to the worklist, so no inlining
            /// opportunities are missed.
            if (result.type == Modification::SplitSCC) {
                worklist.insert(result.modifiedSCCs.begin(),
                                result.modifiedSCCs.end());
                incGuard.disarm();
                return std::nullopt;
            }
        }
    }
    incorporatedFunctions[node.function()].insert(inlined.begin(),
                                                  inlined.end());
    /// If we succeeded, we optimize again to catch optimization
    /// opportunities emerged from inlining.
    if (!modifiedAny) {
        return false;
    }
    modifiedAny |= optimize(*node.function());
    ir::assertInvariants(ctx, *node.function());
    if (handleCalleeRecompute(callGraph.recomputeCallees(node))) {
        return std::nullopt;
    }
    return modifiedAny;
}

static bool callsFunction(ir::Function* caller, ir::Function* callee) {
    for (auto& call: caller->instructions() | Filter<Call>) {
        if (call.function() == callee) {
            return true;
        }
    }
    return false;
}

void Inliner::analyzeSelfRecursion(ir::Function* function) {
    if (callsFunction(function, function)) {
        selfRecursive.insert(function);
    }
}

utl::small_vector<SCC*> Inliner::gatherSinks() {
    utl::small_vector<SCC*> result;
    for (auto* scc: callGraph.sccs()) {
        if (scc->successors().empty()) {
            result.push_back(scc);
        }
    }
    return result;
}

bool Inliner::shouldInlineCallsite(Call const* call, int visitCount) {
    auto* caller = call->parentFunction();
    auto* callee = dyncast<Function const*>(call->function());
    SC_ASSERT(callee, "");
    /// We ignore direct recursion and we don't inline self recursive
    /// functions
    if (caller == callee || selfRecursive.contains(callee)) {
        return false;
    }
    /// If the caller is being revisited and we inlined the callee before, we
    /// will only inline again if it's a leaf function
    if (visitCount > 0 && incorporatedFunctions[caller].contains(callee) &&
        !callGraph[callee]->isLeaf())
    {
        return false;
    }
    ssize_t calleeNumInstructions = ranges::distance(callee->instructions());
    /// Most naive heuristic ever: Inline if we have less than 14 instructions.
    if (calleeNumInstructions < 40) {
        return true;
    }
    /// If we have constant arguments, then there are more opportunities for
    /// optimization, so we inline more aggressively.
    if (ranges::any_of(call->arguments(), isa<Constant>)) {
        if (calleeNumInstructions < 21) {
            return true;
        }
    }
    /// Also always inline if we are the only user of this function.
    if (callee->users().size() <= 1) {
        return true;
    }
    return false;
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
        if (!functionPass(ctx, function)) {
            break;
        }
        modifiedAny = true;
    }
    return modifiedAny;
}

bool Inliner::handleCalleeRecompute(
    SCCCallGraph::RecomputeCalleesResult result) {
    if (!result) {
        return false;
    }
    worklist.insert(result.modifiedSCCs.begin(), result.modifiedSCCs.end());
    auto newCalleeSSCs = result.newCallees | transform(&FunctionNode::scc);
    worklist.insert(newCalleeSSCs.begin(), newCalleeSSCs.end());
    return true;
}

void Inliner::printWorklist() const {
    auto indent = [&](bool begin = false) {
        if (begin) {
            std::cout << " -  ";
        }
        else {
            std::cout << "    ";
        }
    };
    for (auto* scc: worklist) {
        for (auto [index, node]: scc->nodes() | enumerate) {
            indent(index == 0);
            std::cout << format(*node->function()) << std::endl;
        }
        indent();
        std::cout << "All callees analyzed: "
                  << (allSuccessorsAnalyzed(*scc) ? "true" : "false")
                  << std::endl;
    }
}

using namespace tfmt::modifiers;

void Inliner::printVisit(Function const& F) {
    if (!args.printVisOrder) {
        return;
    }
    std::cout << "  " << tfmt::format(Grey | Bold, "Visiting: ") << format(F)
              << std::endl;
}

void Inliner::printDecision(Call const& inst, bool decision) {
    if (!args.printDecisions) {
        return;
    }
    std::cout << "    ";
    if (decision) {
        std::cout << tfmt::format(Green | Bold, "Inlining: ");
    }
    else {
        std::cout << tfmt::format(Red | Bold, "Not inlining: ");
    }
    std::cout << format(inst) << "\n";
}
