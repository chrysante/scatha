/// # Before loop rotation
///
/// `H` is the loop header. If `H` has multiple predecessors that are not loop
/// nodes, a preheader is inserted so `H` has one edge coming from outside the
/// loop. `H` is expected to have one edge going into the loop and one edge
/// leaving the loop. `E` (entry) is the successor of `H` that is a loop node.
/// If that node has multiple predecessors a new node is inserted to take the
/// role of `E`. Nodes marked with `X` are loop nodes without any special role.
/// These could be exiting nodes. Exiting nodes can exit to `S` (skip block, the
/// other successor of `H`) or to other nodes. If `S` has multiple predecessors,
/// a new node will be inserted for `S`
///
///
///           ┌┴─┴┐           ┌───┐
///           │   ├──────────>│ S │
///       ┌──>│ H │<───┐      └───┘
///       │   └─┬─┘    │
///       │     ↓      │
///       │   ┌───┐    │
///       │   │ E │    │
///       │   └─┬─┘    │
///       │    ┌┴┐     │
///       │    │ │     │
///       │    │ │     │
///     ┌─┴─┐  │ │   ┌─┴─┐
///     │ X │<─┘ └──>│ X │
///     └─┬─┘        └───┘
///       │
///       ↓
///
/// # After loop rotation
///
/// `F` (footer) is a copy of `H`. `H` is renamed to `G` (guard). All
/// predecessors of `H` that are loop nodes now point to `F`. An edge from `F`
/// to `E` is added. This means now `E` has multiple predecessors and thus
/// (likely) has phi instructions.
///
///               ┌─┴─┐
///               │ G ├────────────┐
///               └─┬─┘            │
///                 ↓              ↓
///               ┌───┐          ┌───┐
///               │ E │<───────┐ │ S │
///               └─┬─┘        │ └───┘
///                ┌┴┐         │   ↑
///                │ │         │   │
///                │ │         │   │
///     ┌───┐      │ │   ┌───┐ │   │
///     │ X │<─────┘ └──>│ X │ │   │
///     └┬┬─┘      ┌───┐ └─┬─┘ │   │
///      ││        │   │<──┘   │   │
///      │└───────>│ F │───────┘   │
///      ↓         │   │───────────┘
///                └───┘
///
/// # Implementation
///
/// ## Preprocessing
/// We add a preheader and a new nodes `E` and `S` if necessary.
///
/// ## Rotation
/// - We add single value phi instructions to `E` and to `S` for every
///   instruction in `H`. We replace all uses of the instructions in `H` that
///   are dominated by `E` and `S` with the corresponding phi instruction.
/// - We clone `H`, name the clone `F` and rename `H` to `G`. `F` now has the
///   same successors as `G`. This will remain so. We register `F` as a
///   predecessor of `E` and `S` and set all the phi instructions in these
///   blocks accordingly. In particular, there is one phi instruction for every
///   instruction in `G`. If previously there were other single valued phis in
///   `S` or `E` we replaced them by their argument. Therefore we can simply
///   add an entry to each phi instruction with the corresponding instruction in
///   `F`.
/// - All predecessors of `G` that are loop nodes are rewired to `F`. Since `F`
///   has a phi instruction for every phi in `G` we can keep these and use them
///   for the loop predecessors of `G`.
/// - After the rotation `E` is a loop header. If `E` is a while loop we perform
///   the preprocessing and transform again on `E`.
///

#include "Opt/Passes.h"

#include <queue>

#include <utl/hashtable.hpp>
#include <utl/strcat.hpp>
#include <utl/vector.hpp>

#include "Common/Ranges.h"
#include "IR/CFG.h"
#include "IR/Clone.h"
#include "IR/Dominance.h"
#include "IR/Loop.h"
#include "IR/PassRegistry.h"
#include "IR/Type.h"
#include "IR/Validate.h"
#include "Opt/Common.h"
#include "Opt/LoopRankView.h"

using namespace scatha;
using namespace opt;
using namespace ir;
using namespace ranges::views;

SC_REGISTER_FUNCTION_PASS(opt::loopRotate, "looprotate",
                          PassCategory::Canonicalization, {});

/// \Returns `true` if \p node is a while loop, i.e., if the header has a
/// successor that is not a loop node
static bool isWhileLoop(LNFNode const* header, LoopNestingForest const& LNF) {
    if (!header->isProperLoop()) {
        return false;
    }
    if (header->children().empty()) {
        return false;
    }
    return ranges::any_of(header->basicBlock()->successors(),
                          [&](BasicBlock const* succ) {
        return !LNF[succ]->isLoopNodeOf(header);
    });
}

namespace {

struct PreprocessResult {
    BasicBlock* entry;
    BasicBlock* skip;
    utl::small_vector<BasicBlock*> headerLoopPreds;
    utl::small_vector<BasicBlock*> headerNonLoopPreds;
};

struct LRContext {
    Context& ctx;
    Function& function;
    LoopNestingForest& LNF;
    DominanceInfo const* domInfo;

    utl::small_vector<Phi*> addedPhis;
    utl::hashmap<Instruction*, Phi*> headerToSkipPhis;
    utl::hashmap<Instruction*, Phi*> headerToEntryPhis;

    /// \Returns `true` if \p dom dominates \p sub
    /// Should only be used with `entry` and `skip` blocks
    bool dominates(BasicBlock const* dom, BasicBlock const* sub) const {
        auto& dominatorSet = domInfo->dominatorSet(sub);
        return dominatorSet.contains(dom);
    }

    std::array<utl::small_vector<BasicBlock*>, 2> partitionLoopPreds(
        BasicBlock* BB, BasicBlock const* header);

    LRContext(Context& ctx, Function& function):
        ctx(ctx),
        function(function),
        LNF(function.getOrComputeLNF()),
        domInfo(&function.getOrComputeDomInfo()) {}

    void rotate(BasicBlock* header);

    BasicBlock* addPreheader(BasicBlock* header,
                             std::span<BasicBlock* const> nonLoopPreds);
    void recomputeDomInfo();

    PreprocessResult preprocess(BasicBlock* header);
    utl::hashmap<Instruction*, Phi*> mapInstructionsToPhis(BasicBlock* header,
                                                           BasicBlock* succ);
    void updateDominatedUsers(Instruction& inst, BasicBlock* succ, Value* repl);
    void addFooterAsPred(BasicBlock* footer, BasicBlock* header,
                         BasicBlock* succ,
                         utl::hashmap<Instruction*, Phi*> const& instPhiMap);
    void cleanPhiNodes();
};

} // namespace

bool opt::loopRotate(Context& ctx, Function& function) {
    auto& LNF = function.getOrComputeLNF();
    auto LRV = LoopRankView::Compute(function, [&](BasicBlock const* header) {
        return isWhileLoop(LNF[header], LNF);
    });
    /// We traverse all while loops in rank order (BFS order)
    bool modified = false;
    for (auto& rankList: LRV) {
        for (auto* header: rankList) {
            LRContext(ctx, function).rotate(header);
        }
        /// After traversing a rank we invalidate because we may have edited
        /// the CFG in loops the are dominated by the next rank
        if (!rankList.empty()) {
            function.invalidateCFGInfo();
            modified = true;
        }
    }
    assertInvariants(ctx, function);
    return modified;
}

/// \Returns all basic blocks in \p BBs that satisfy \p pred in the first vector
/// and all other blocks in the second vector
static std::array<utl::small_vector<BasicBlock*>, 2> partitionBlocks(
    std::span<BasicBlock* const> BBs,
    utl::function_view<bool(BasicBlock const*)> pred) {
    std::array<utl::small_vector<BasicBlock*>, 2> result;
    auto& [yes, no] = result;
    for (auto* BB: BBs) {
        if (pred(BB)) {
            yes.push_back(BB);
        }
        else {
            no.push_back(BB);
        }
    }
    return result;
}

/// \Returns all predecessors of \p BB that are loop nodes of \p headerNode
/// in the first vector and all other predecessors in the second vector
std::array<utl::small_vector<BasicBlock*>, 2> LRContext::partitionLoopPreds(
    BasicBlock* BB, BasicBlock const* header) {
    return partitionBlocks(BB->predecessors(), [&](BasicBlock const* pred) {
        /// I have no idea why this is correct but apparently it is...
        if (!dominates(header, pred)) {
            return false;
        }
        if (header == BB) {
            return true;
        }
        return !dominates(BB, pred);
    });
}

void LRContext::rotate(BasicBlock* header) {
    auto [entry, skip, loopPreds, nonLoopPreds] = preprocess(header);

    /// # Step 1
    /// `mapInstructionsToPhis()` replaces all uses of the instructions in the
    /// header that are dominated by either `entry` or `skip` with phi nodes in
    /// the respective block. The set of nodes dominated by `entry`, the set of
    /// nodes dominated by skip and `header` partition the dom set of the
    /// header. So all uses of instructions in the header that are not in in the
    /// header will be replaced by the single value phi nodes.
    headerToSkipPhis = mapInstructionsToPhis(header, skip);
    headerToEntryPhis = mapInstructionsToPhis(header, entry);

    /// # Step 2
    /// We clone the header and rename the nodes
    auto* footer = ir::clone(ctx, header).release();
    footer->setName("loop.footer");
    header->setName("loop.guard");
    function.insert(skip, footer);
    auto* guard = header;

    /// We add arguments for `footer` to the phi instructions of `entry` and
    /// `skip` and register `footer` as a predecessor
    addFooterAsPred(footer, header, entry, headerToEntryPhis);
    addFooterAsPred(footer, header, skip, headerToSkipPhis);

    /// # Step 3
    /// We remove all the loop predecessors of `guard` and point them to
    /// `footer`. `footer` already has phi instructions for the predecessors
    /// because `header` also had them
    for (auto* pred: loopPreds) {
        pred->terminator()->updateTarget(guard, footer);
        guard->removePredecessor(pred);
    }
    /// We unregister the non-loop predecessors from `footer`
    for (auto* pred: nonLoopPreds) {
        footer->removePredecessor(pred);
    }

    /// We check if `footer` has any self referential phi nodes. For `header` it
    /// was okay to have self referential phi nodes for blocks it dominated. But
    /// `footer` does not dominate any of the loop nodes, so we replace self
    /// referential arguments with the correspoding value in `entry`.
    utl::hashmap<Instruction*, Instruction*> FtoE;
    for (auto&& [F, E]: zip(*footer, *entry)) {
        FtoE[&F] = &E;
    }
    for (auto& phi: footer->phiNodes()) {
        for (auto [index, arg]: phi.operands() | enumerate) {
            auto* instArg = dyncast<Instruction*>(arg);
            if (instArg && instArg->parent() == footer) {
                phi.setOperand(index, FtoE[instArg]);
            }
        }
    }

    /// We remove all the phi nodes that were added for no reason
    cleanPhiNodes();

    function.invalidateDomInfo();
}

BasicBlock* LRContext::addPreheader(BasicBlock* header,
                                    std::span<BasicBlock* const> nonLoopPreds) {
    return addJoiningPredecessor(ctx, header, nonLoopPreds, "preheader");
}

void LRContext::recomputeDomInfo() {
    function.invalidateDomInfo();
    domInfo = &function.getOrComputeDomInfo();
}

PreprocessResult LRContext::preprocess(BasicBlock* header) {
    LNFNode const* headerNode = LNF[header];
    /// Partition the predecessors of `header` into loop predecessors and
    /// non-loop predecessors.
    auto [loopPreds, nonLoopPreds] = partitionLoopPreds(header, header);
    if (nonLoopPreds.size() > 1) {
        auto* preheader = addPreheader(header, nonLoopPreds);
        nonLoopPreds = { preheader };
    }
    /// We now determine which successor is `E` and which is `S`
    SC_ASSERT(header->numSuccessors() == 2,
              "If we have one successors this is either not a loop header or "
              "not a while loop and if we have more than 2 successors this is "
              "a weird switch based loop that we don't support.");
    auto [entry, skip] = [&] {
        auto A = header->successor(0);
        auto B = header->successor(1);
        if (LNF[A]->isLoopNodeOf(headerNode)) {
            return std::pair{ A, B };
        }
        return std::pair{ B, A };
    }();
    bool domInfoInvalidated = false;
    /// We add new nodes for `entry` and `skip` if necessary
    if (entry->numPredecessors() > 1) {
        entry = splitEdge("loop.entry", ctx, header, entry);
        domInfoInvalidated = true;
    }
    auto [skipLoopPreds, skipNonLoopPreds] = partitionLoopPreds(skip, header);
    if (!skipNonLoopPreds.empty()) {
        SC_ASSERT(!skipLoopPreds.empty(), "");
        skip = addJoiningPredecessor(ctx, skip, skipLoopPreds, "skip");
        domInfoInvalidated = true;
    }
    if (domInfoInvalidated) {
        recomputeDomInfo();
    }
    return { entry, skip, loopPreds, nonLoopPreds };
}

template <typename Container>
static Container gatherExistingPhiNodes(BasicBlock* BB) {
    Container phis;
    for (auto& phi: BB->phiNodes()) {
        if constexpr (requires { phis.push_back(&phi); }) {
            phis.push_back(&phi);
        }
        else if constexpr (requires { phis.insert(&phi); }) {
            phis.insert(&phi);
        }
        else {
            static_assert(!std::is_same_v<Container, Container>);
        }
    }
    return phis;
}

utl::hashmap<Instruction*, Phi*> LRContext::mapInstructionsToPhis(
    BasicBlock* header, BasicBlock* succ) {
    SC_EXPECT(succ->isPredecessor(header));
    utl::hashmap<Instruction*, Phi*> map;
    auto existingPhis = gatherExistingPhiNodes<utl::small_vector<Phi*>>(succ);
    for (auto& inst: *header) {
        if (isa<VoidType>(inst.type())) {
            continue;
        }
        if (isa<TerminatorInst>(inst)) {
            break;
        }
        Phi* phi = nullptr;
        auto itr = ranges::find_if(existingPhis, [&](Phi* phi) {
            return phi->operandOf(header) == &inst;
        });
        if (itr != existingPhis.end()) {
            phi = *itr;
            existingPhis.erase(itr);
        }
        else {
            auto args = succ->predecessors() | transform([&](BasicBlock* pred) {
                return PhiMapping{ pred, &inst };
            }) | ToSmallVector<>;
            phi = new Phi(args, std::string(inst.name()));
            succ->insertPhi(phi);
            addedPhis.push_back(phi);
        }
        map[&inst] = phi;
        updateDominatedUsers(inst, succ, phi);
    }
    return map;
}

void LRContext::updateDominatedUsers(Instruction& inst, BasicBlock* succ,
                                     Value* repl) {
    for (auto* user: inst.users() | ToSmallVector<>) {
        if (user->parent() == succ && isa<Phi>(user)) {
            continue;
        }
        if (auto* phi = dyncast<Phi*>(user)) {
            for (auto [index, arg]: phi->arguments() | enumerate) {
                auto [pred, operand] = arg;
                if (operand == &inst && dominates(succ, pred)) {
                    phi->setOperand(index, repl);
                }
            }
            continue;
        }
        if (dominates(succ, user->parent())) {
            user->updateOperand(&inst, repl);
        }
    }
}

void LRContext::addFooterAsPred(
    BasicBlock* footer, BasicBlock* header, BasicBlock* succ,
    utl::hashmap<Instruction*, Phi*> const& instPhiMap) {
    succ->addPredecessor(footer);
    auto phiNodes = gatherExistingPhiNodes<utl::hashset<Phi*>>(succ);
    for (auto [footerInst, headerInst]: zip(*footer, *header)) {
        if (isa<VoidType>(headerInst.type())) {
            continue;
        }
        if (isa<TerminatorInst>(headerInst)) {
            break;
        }
        auto itr = instPhiMap.find(&headerInst);
        SC_ASSERT(itr != instPhiMap.end(), "");
        auto* phi = itr->second;
        phi->addArgument(footer, &footerInst);
        phiNodes.erase(phi);
    }
    for (auto* otherPhi: phiNodes) {
        otherPhi->addArgument(footer, otherPhi->operandOf(header));
    }
}

void LRContext::cleanPhiNodes() {
    for (auto* phi: addedPhis) {
        if (phi->unused()) {
            phi->parent()->erase(phi);
            continue;
        }
        if (phi->numOperands() == 1) {
            phi->replaceAllUsesWith(phi->operandAt(0));
            phi->parent()->erase(phi);
        }
    }
}
