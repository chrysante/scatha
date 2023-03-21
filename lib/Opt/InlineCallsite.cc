#include "Opt/InlineCallsite.h"

#include <utl/vector.hpp>

#include "IR/CFG.h"
#include "IR/Clone.h"
#include "IR/Context.h"
#include "Opt/Common.h"

using namespace scatha;
using namespace ir;
using namespace opt;

void opt::inlineCallsite(ir::Context& ctx, FunctionCall* call) {
    auto* callerBB   = call->parent();
    auto* caller     = callerBB->parent();
    auto* callee     = call->function();
    auto calleeClone = clone(ctx, callee);
    for (auto& bb: *calleeClone) {
        bb.setName(ctx.uniqueName(caller, bb.name()));
        for (auto& inst: bb) {
            inst.setName(ctx.uniqueName(caller, inst.name()));
        }
    }
    auto* newGoto = new Goto(ctx, &calleeClone->entry());
    calleeClone->entry().setPredecessors(std::array{ callerBB });
    callerBB->insert(call, newGoto);
    auto* landingpad =
        new BasicBlock(ctx, ctx.uniqueName(caller, "inline.landingpad"));
    landingpad->splice(landingpad->begin(),
                       BasicBlock::Iterator(newGoto->next()),
                       callerBB->end());
    for (auto* succ: landingpad->successors()) {
        succ->updatePredecessor(callerBB, landingpad);
        for (auto& phi: succ->phiNodes()) {
            size_t const index = phi.indexOf(callerBB);
            phi.setPredecessor(index, landingpad);
        }
    }
    caller->insert(callerBB->next(), landingpad);
    /// Replace all parameters with the callers arguments.
    for (auto [param, arg]:
         ranges::views::zip(calleeClone->parameters(), call->arguments()))
    {
        replaceValue(&param, arg);
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
        ranges::to<utl::small_vector<BasicBlock*>>);
    if (!isa<VoidType>(callee->returnType())) {
        /// Add a phi node to `landingpad` the merge all returns from
        /// `calleeClone`
        auto* phi = new Phi(call->type(), ctx.uniqueName(caller, "inline.phi"));
        phi->setArguments(phiArgs);
        landingpad->insert(landingpad->begin(), phi);
        replaceValue(call, phi);
    }
    /// Now that we have eventually replaced all uses with a phi node we can
    /// erase the call instruction.
    landingpad->erase(call);
    /// Move basic blocks from the calleeClone into calling function.
    caller->splice(Function::Iterator(landingpad), calleeClone.get());
}
