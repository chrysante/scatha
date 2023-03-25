#include "Opt/SimplifyCFG.h"

#include <utl/hashtable.hpp>
#include <utl/scope_guard.hpp>

#include "IR/CFG.h"
#include "IR/Validate.h"
#include "Opt/Common.h"

using namespace scatha;
using namespace opt;
using namespace ir;

namespace {

struct Ctx {
    Ctx(ir::Context& irCtx, Function& function):
        irCtx(irCtx), function(function) {}

    void replaceConstCondBranches(BasicBlock* bb);

    void eraseUnreachableBlocks();

    void removeDeadLink(BasicBlock* origin, BasicBlock* dest);

    void merge();

    void eraseDeadBasicBlock(BasicBlock* bb);

    /// \returns `nullptr` if \p bb could not be erased or a basic block to
    /// push into the worklist otherwise.
    BasicBlock* eraseForwardingBasicBlock(BasicBlock* bb);

    ir::Context& irCtx;
    Function& function;
    utl::hashset<BasicBlock const*> visited;
    bool changedAny = false;
};

} // namespace

bool opt::simplifyCFG(ir::Context& irCtx, Function& function) {
    Ctx ctx(irCtx, function);
    ctx.replaceConstCondBranches(&function.entry());
    ctx.eraseUnreachableBlocks();
    ctx.visited.clear();
    ctx.merge();
    assertInvariants(irCtx, function);
    return ctx.changedAny;
}

void Ctx::replaceConstCondBranches(BasicBlock* bb) {
    if (visited.contains(bb)) {
        return;
    }
    visited.insert(bb);
    // clang-format off
    visit(*bb->terminator(), utl::overload{
        [&](Goto const&) {},
        [&](Return const&) {},
        [&](Branch& branch) {
            if (auto* cond = dyncast<IntegralConstant const*>(branch.condition())) {
                auto const value = cond->value().to<ssize_t>();
                SC_ASSERT(value <= 1, "");
                /// First target will be taken if `value == 1` aka `cond == true`
                /// This means the branch to target at index `value` shall be removed.
                auto* deadSuccessor = branch.targets()[value];
                auto* liveSuccessor = branch.targets()[1 - value];
                removeDeadLink(bb, deadSuccessor);
                bb->erase(&branch);
                bb->insert(bb->end(), new Goto(irCtx, liveSuccessor));
            }
        },
    }); // clang-format on
    for (auto* succ: bb->successors()) {
        replaceConstCondBranches(succ);
    }
}

void Ctx::eraseUnreachableBlocks() {
    auto unreachableBlocks =
        function | ranges::views::filter([&](auto& bb) {
            return !visited.contains(&bb);
        }) |
        ranges::views::transform([](auto& bb) { return &bb; }) |
        ranges::to<utl::small_vector<BasicBlock*>>;
    for (auto* bb: unreachableBlocks) {
        for (auto* succ: bb->successors()) {
            if (succ) {
                succ->removePredecessor(bb);
            }
        }
        for (auto& inst: *bb) {
            clearAllUses(&inst);
        }
        clearAllUses(bb);
        function.erase(bb);
    }
}

void Ctx::removeDeadLink(BasicBlock* origin, BasicBlock* dest) {
    origin->terminator()->updateTarget(dest, nullptr);
    if (dest->hasSinglePredecessor()) {
        SC_ASSERT(dest->singlePredecessor() == origin, "Bad link");
        eraseDeadBasicBlock(dest);
    }
    else if (origin) {
        dest->removePredecessor(origin);
    }
}

void Ctx::merge() {
    utl::hashset<BasicBlock*> worklist = { &function.entry() };
    utl::hashmap<BasicBlock*, bool> visited;
    auto insertSuccessors = [&](BasicBlock* bb) {
        auto succlist = bb->successors();
        worklist.insert(succlist.begin(), succlist.end());
    };
    while (!worklist.empty()) {
        auto* bb = *worklist.begin();
        worklist.erase(worklist.begin());
        if (visited[bb]) {
            continue;
        }
        visited[bb] = true;
        /// Replace a `branch` with all equal targets by a `goto`
        /// I guess we will have to generalize this once we get `switch`
        /// instructions.
        if (bb->successors().size() == 2 &&
            bb->successor(0) == bb->successor(1))
        {
            auto* succ = bb->successor(0);
            SC_ASSERT(succ->phiNodes().empty(),
                      "This case should not happen with phi nodes in `succ`");
            succ->removePredecessor(bb);
            SC_ASSERT(ranges::find(succ->predecessors(), bb) !=
                          succ->predecessors().end(),
                      "`bb` should have been a duplicate predecessor of `succ` "
                      "and should thus be still in the list after erasing one "
                      "pointer ");
            auto* newTerm = new Goto(irCtx, succ);
            bb->erase(bb->terminator());
            bb->pushBack(newTerm);
        }
        if (!bb->hasSingleSuccessor()) {
            insertSuccessors(bb);
            continue;
        }
        auto* succ = bb->singleSuccessor();
        if (auto* bb2 = eraseForwardingBasicBlock(bb)) {
            /// To visit `pred` again we need to mark it unvisited.
            visited[bb2] = false;
            worklist.insert(bb2);
            continue;
        }
        if (!succ->hasSinglePredecessor()) {
            insertSuccessors(bb);
            continue;
        }
        SC_ASSERT(succ->singlePredecessor() == bb,
                  "BBs are not linked properly");
        /// Now we have the simple case we are looking for. Here we can merge
        /// the basic blocks.
        bb->erase(bb->terminator());
        for (auto& phi: succ->phiNodes()) {
            SC_ASSERT(phi.argumentCount() == 1, "Invalid argument count");
            opt::replaceValue(&phi, phi.argumentAt(0).value);
        }
        succ->eraseAllPhiNodes();
        bb->splice(bb->end(), succ);
        for (auto* newSucc: bb->successors()) {
            newSucc->updatePredecessor(succ, bb);
        }
        function.erase(succ);
        /// We process ourself again, because otherwise we miss single
        /// successors of this.
        visited[bb] = false;
        worklist.insert(bb);
    }
}

void Ctx::eraseDeadBasicBlock(BasicBlock* bb) {
    for (auto* succ: bb->successors()) {
        removeDeadLink(bb, succ);
    }
    function.erase(bb);
}

BasicBlock* Ctx::eraseForwardingBasicBlock(BasicBlock* bb) {
    /// Erase empty basic block which just 'forward' meaning one incoming
    /// and one outgoing edge.
    if (!bb->hasSinglePredecessor() || !bb->hasSingleSuccessor() ||
        !bb->emptyExceptTerminator())
    {
        return nullptr;
    }
    /// Simple `Pred -> BB -> Succ` case where `BB` is empty.
    /// Maybe we can just erase `BB` and branch to `Succ` directly.
    auto* succ = bb->singleSuccessor();
    auto* pred = bb->singlePredecessor();
    /// If `succ` doesn't have phi nodes we have nothing to worry about.
    if (succ->phiNodes().empty()) {
        goto doErase;
    }
    if (ranges::find(succ->predecessors(), pred) ==
        ranges::end(succ->predecessors()))
    {
        goto doErase;
    }
    /// Our predecessor, which would become `succ`'s predecessor, is already
    /// a predecessor of `succ` and would thus become a duplicate
    /// predecessor of `succ`. Since `succ` has phi nodes, this is a
    /// problem as they become ambigous.
    SC_ASSERT(succ->numPredecessors() > 1, "Can hardly be 1 or 0");
    if (succ->numPredecessors() == 2 && pred->numSuccessors() == 2) {
        /// We got lucky and we can replace the `phi`'s in `succ` by `select`
        /// instructions.
        /// `pred`'s terminator  must be a `branch` since `pred` has 2
        /// successors.
        auto* branch = cast<Branch*>(pred->terminator());
        utl::small_vector<Select*> selects;
        auto* a = pred;
        auto* b = bb;
        if (branch->thenTarget() == b) {
            std::swap(a, b);
        }
        for (auto& phi: succ->phiNodes()) {
            auto* select = new Select(branch->condition(),
                                      phi.operandOf(a),
                                      phi.operandOf(b),
                                      utl::strcat("select.", phi.name()));
            selects.push_back(select);
            opt::replaceValue(&phi, select);
        }
        succ->eraseAllPhiNodes();
        for (auto* select: selects | ranges::views::reverse) {
            succ->pushFront(select);
        }
        goto doErase;
    }
    return nullptr;
doErase:
    pred->terminator()->updateTarget(bb, succ);
    succ->updatePredecessor(bb, pred);
    function.erase(bb);
    return pred;
}
