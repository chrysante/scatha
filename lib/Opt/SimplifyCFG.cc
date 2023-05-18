#include "Opt/SimplifyCFG.h"

#include <range/v3/algorithm.hpp>
#include <utl/hashtable.hpp>
#include <utl/scope_guard.hpp>
#include <utl/strcat.hpp>

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

    bool replaceConstCondBranches(BasicBlock* bb);

    bool eraseUnreachableBlocks();

    void removeDeadLink(BasicBlock* origin, BasicBlock* dest);

    bool mainPass();

    void eraseDeadBasicBlock(BasicBlock* bb);

    bool merge(BasicBlock* pred, BasicBlock* via, BasicBlock* succ);

    ir::Context& irCtx;
    Function& function;
    utl::hashset<BasicBlock const*> visited;
};

} // namespace

bool opt::simplifyCFG(ir::Context& irCtx, Function& function) {
    Ctx ctx(irCtx, function);
    bool modifiedAny = false;
    modifiedAny |= ctx.replaceConstCondBranches(&function.entry());
    modifiedAny |= ctx.eraseUnreachableBlocks();
    ctx.visited.clear();
    modifiedAny |= ctx.mainPass();
    assertInvariants(irCtx, function);
    if (modifiedAny) {
        function.invalidateCFGInfo();
    }
    return modifiedAny;
}

bool Ctx::replaceConstCondBranches(BasicBlock* bb) {
    if (visited.contains(bb)) {
        return false;
    }
    visited.insert(bb);
    // clang-format off
    bool modified = visit(*bb->terminator(), utl::overload{
        [&](Goto const&) { return false; },
        [&](Return const&) { return false; },
        [&](Branch& branch) {
            auto* constCond =
                dyncast<IntegralConstant const*>(branch.condition());
            if (!constCond) {
                return false;
            }
            auto const value = constCond->value().to<ssize_t>();
            SC_ASSERT(value <= 1, "");
            /// First target will be taken if `value == 1` aka
            /// `constCond == true`. This means the branch to target at index
            /// `value` shall be removed.
            auto* deadSuccessor = branch.targets()[value];
            auto* liveSuccessor = branch.targets()[1 - value];
            removeDeadLink(bb, deadSuccessor);
            bb->erase(&branch);
            bb->insert(bb->end(), new Goto(irCtx, liveSuccessor));
            return true;
        },
    }); // clang-format on
    for (auto* succ: bb->successors()) {
        modified |= replaceConstCondBranches(succ);
    }
    return modified;
}

bool Ctx::eraseUnreachableBlocks() {
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
    return !unreachableBlocks.empty();
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

bool Ctx::mainPass() {
    utl::hashset<BasicBlock*> worklist = { &function.entry() };
    utl::hashmap<BasicBlock*, bool> visited;
    auto insertSuccessors = [&](BasicBlock* bb) {
        auto succlist = bb->successors();
        worklist.insert(succlist.begin(), succlist.end());
    };
    bool modified = false;
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
            modified = true;
        }
        if (!bb->hasSingleSuccessor()) {
            insertSuccessors(bb);
            continue;
        }
        auto* succ = bb->singleSuccessor();
        if (bb->emptyExceptTerminator()) {
            /// This is a forwarding basic block. We can try to redirect
            /// predecessors directly to the successor.
            bool all = true, any = false;
            for (auto* pred: bb->predecessors()) {
                if (merge(pred, bb, succ)) {
                    /// To visit `pred` again we need to mark it unvisited.
                    visited[pred] = false;
                    worklist.insert(pred);
                    modified = true;
                    any      = true;
                    continue;
                }
                all = false;
            }
            if (any && all) {
                /// `bb` might have been erased as a predecessor of `succ`
                if (succ->isPredecessor(bb)) {
                    succ->removePredecessor(bb);
                }
                function.erase(bb);
                continue;
            }
        }
        if (!succ->hasSinglePredecessor()) {
            insertSuccessors(bb);
            continue;
        }
        auto* pred = bb->singlePredecessor();
        SC_ASSERT(succ->singlePredecessor() == bb,
                  "BBs are not linked properly");
        /// Don't want to destroy loops
        if (succ == pred) {
            continue;
        }
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
        worklist.erase(succ);
        /// We process ourself again, because otherwise we miss single
        /// successors of this.
        visited[bb] = false;
        worklist.insert(bb);
        modified = true;
    }
    return modified;
}

void Ctx::eraseDeadBasicBlock(BasicBlock* bb) {
    for (auto* succ: bb->successors()) {
        removeDeadLink(bb, succ);
    }
    function.erase(bb);
}

bool Ctx::merge(BasicBlock* pred, BasicBlock* via, BasicBlock* succ) {
    SC_ASSERT(via->emptyExceptTerminator(), "");
    auto doMerge = [&] {
        auto* predTerm = pred->terminator();
        pred->terminator()->updateTarget(via, succ);
        if (auto* branch = dyncast<Branch*>(predTerm);
            branch && branch->thenTarget() == branch->elseTarget())
        {
            auto* target = branch->thenTarget();
            auto* gt     = new Goto(irCtx, target);
            pred->insert(branch, gt);
            pred->erase(branch);
        }
        succ->addPredecessor(pred);
        for (auto& phi: succ->phiNodes()) {
            phi.addArgument(pred, phi.operandOf(via));
        }
    };
    /// If `succ` doesn't have phi nodes we have nothing to worry about.
    if (succ->phiNodes().empty()) {
        doMerge();
        return true;
    }
    if (ranges::find(succ->predecessors(), pred) ==
        ranges::end(succ->predecessors()))
    {
        doMerge();
        return true;
    }
    /// ** We are a critical edge **
    /// Our predecessor, which would become `succ`'s predecessor, is already
    /// a predecessor of `succ` and would thus become a duplicate
    /// predecessor of `succ`. Since `succ` has phi nodes, this is a
    /// problem as they become ambiguous.
    SC_ASSERT(succ->numPredecessors() > 1, "Can hardly be 1 or 0");
    if (succ->numPredecessors() != 2 || pred->numSuccessors() != 2) {
        return false;
    }
    /// We got lucky and we can replace the `phi`'s in `succ` by `select`
    /// instructions.
    /// `pred`'s terminator  must be a `branch` since `pred` has 2
    /// successors.
    auto* branch = cast<Branch*>(pred->terminator());
    utl::small_vector<Select*> selects;
    auto* a = pred;
    auto* b = via;
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
    succ->removePredecessor(pred);
    succ->removePredecessor(via);
    doMerge();
    return true;
}
