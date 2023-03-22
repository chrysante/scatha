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

    void merge();

    void eraseDeadBasicBlock(BasicBlock* bb);

    void removeDeadLink(BasicBlock* origin, BasicBlock* dest);

    ir::Context& irCtx;
    Function& function;
    utl::hashset<BasicBlock const*> visited;
    bool changedAny = false;
};

} // namespace

bool opt::simplifyCFG(ir::Context& irCtx, Function& function) {
    Ctx ctx(irCtx, function);
    ctx.replaceConstCondBranches(&function.entry());
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

static bool allSuccessorsEqual(BasicBlock const* bb) {
    return ranges::all_of(bb->successors(),
                          [first = bb->successors().front()](auto* succ) {
        return succ == first;
    });
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

        /// Unfortunately the disabled code does not work in general without
        /// `select` instructions. In the following scenario
        /// ```
        ///   A
        ///  / \
        /// B   C
        ///  \ /
        ///   D
        /// ```
        /// when in `D` different values are `phi`'d we cannot collapse this
        /// construct without replace the `phi`by a `select`.
#if 0
        /// Replace a `branch` with all equal targets by a `goto`
        if (bb->successors().size() > 1 &&
            allSuccessorsEqual(bb))
        {
            auto* succ = bb->successors().front();
            auto* newTerm = new Goto(irCtx, succ);
            bb->erase(bb->terminator());
            bb->pushBack(newTerm);
            /// Here we would then also need to update or replace the phi nodes in `succ`.
            succ->setPredecessors(std::array{ bb });
        }
#endif
        if (!bb->hasSingleSuccessor()) {
            insertSuccessors(bb);
            continue;
        }
        auto* succ = bb->singleSuccessor();
#if 0
        /// Erase empty basic block which just 'forward' meaning one incoming
        /// and one outgoing edge.
        if (bb->hasSinglePredecessor() && bb->emptyExceptTerminator()) {
            /// Simple `Pred -> BB -> Succ` case where `BB` is empty.
            /// We can just erase `BB` and branch to `Succ` directly.
            auto* pred = bb->singlePredecessor();
            pred->terminator()->updateTarget(bb, succ);
            succ->updatePredecessor(bb, pred);
            function.erase(bb);
            /// To visit `pred` again we need to mark it unvisited.
            visited[pred] = false;
            worklist.insert(pred);
            continue;
        }
#endif
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

void Ctx::removeDeadLink(BasicBlock* origin, BasicBlock* dest) {
    origin->terminator()->updateTarget(dest, nullptr);
    if (dest->hasSinglePredecessor()) {
        SC_ASSERT(dest->singlePredecessor() == origin, "Bad link");
        eraseDeadBasicBlock(dest);
    }
    else {
        dest->removePredecessor(origin);
    }
}
