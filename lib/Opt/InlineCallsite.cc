#include "Opt/InlineCallsite.h"

#include <range/v3/algorithm.hpp>
#include <utl/vector.hpp>

#include "Common/Ranges.h"
#include "IR/CFG.h"
#include "IR/Clone.h"
#include "IR/Context.h"
#include "IR/Type.h"
#include "IR/Validate.h"
#include "Opt/Common.h"

using namespace scatha;
using namespace ir;
using namespace opt;

void opt::inlineCallsite(ir::Context& ctx, Call* call) {
    auto* callee = cast<Function*>(call->function());
    inlineCallsite(ctx, call, ir::clone(ctx, callee));
}

void opt::inlineCallsite(ir::Context& ctx,
                         ir::Call* call,
                         UniquePtr<ir::Function> calleeClone) {
    auto* callerBB = call->parent();
    auto* caller = callerBB->parent();
    auto* newGoto = new Goto(ctx, &calleeClone->entry());
    calleeClone->entry().setPredecessors(std::array{ callerBB });
    callerBB->insert(call, newGoto);
    auto* landingpad = new BasicBlock(ctx, "inline.landingpad");
    landingpad->splice(landingpad->begin(),
                       BasicBlock::Iterator(newGoto->next()),
                       callerBB->end());
    for (auto* succ: landingpad->successors()) {
        succ->updatePredecessor(callerBB, landingpad);
    }
    caller->insert(callerBB->next(), landingpad);
    /// Replace all parameters with the callers arguments.
    for (auto [param, arg]:
         ranges::views::zip(calleeClone->parameters(), call->arguments()))
    {
        param.replaceAllUsesWith(arg);
    }
    /// Traverse `calleeClone` and replace all returns with
    /// gotos to `landingpad`
    utl::small_vector<PhiMapping> phiArgs;
    for (auto& bb: *calleeClone) {
        for (auto itr = bb.begin(); itr != bb.end();) {
            auto* ret = dyncast<Return*>(itr.to_address());
            if (!ret) {
                ++itr;
                continue;
            }
            auto* gotoInst = new Goto(ctx, landingpad);
            bb.insert(ret, gotoInst);
            phiArgs.push_back({ &bb, ret->value() });
            itr = bb.erase(BasicBlock::Iterator(ret));
        }
    }
    landingpad->setPredecessors(
        phiArgs | ranges::views::transform([](auto m) { return m.pred; }) |
        ToSmallVector<>);
    if (!isa<VoidType>(calleeClone->returnType())) {
        /// Add a phi node to `landingpad` the merge all returns from
        /// `calleeClone`
        auto* phi = new Phi(call->type(), "inline.phi");
        phi->setArguments(phiArgs);
        landingpad->insert(landingpad->begin(), phi);
        call->replaceAllUsesWith(phi);
    }
    /// Now that we have eventually replaced all uses with a phi node we can
    /// erase the call instruction.
    landingpad->erase(call);
    /// Move all allocas from the callee into the entry of the caller
    auto allocas =
        calleeClone->entry() | Filter<Alloca> | TakeAddress | ToSmallVector<>;
    auto insertPoint = ranges::find_if(caller->entry(), [](auto& inst) {
        return !isa<Alloca>(inst);
    });
    for (auto* allocaInst: allocas) {
        calleeClone->entry().extract(allocaInst).release();
        caller->entry().insert(insertPoint, allocaInst);
    }
    /// Move basic blocks from the calleeClone into calling function.
    caller->splice(Function::Iterator(landingpad), calleeClone.get());
    caller->invalidateCFGInfo();
}
