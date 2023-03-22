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

    void merge(BasicBlock* bb);

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
    ctx.merge(&function.entry());
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

void Ctx::merge(BasicBlock* bb) {
    if (visited.contains(bb)) {
        return;
    }
    utl::armed_scope_guard visitPreds = [&] {
        for (auto* succ: bb->successors()) {
            merge(succ);
        }
    };
    visited.insert(bb);
    if (!bb->hasSingleSuccessor()) {
        return;
    }
    auto* succ = bb->singleSuccessor();
    if (!succ->hasSinglePredecessor()) {
        return;
    }
    visitPreds.disarm();
    SC_ASSERT(succ->singlePredecessor() == bb, "BBs are not linked properly");
    /// Now we have the simple case we are looking for. Here we can merge the
    /// basic blocks.
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
    /// We process ourself again, because other wise we miss single successors
    /// of this.
    visited.erase(bb);
    merge(bb);
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
