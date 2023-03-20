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
    auto* callingBB       = call->parent();
    auto* callingFunction = callingBB->parent();
    auto* calledFunction  = call->function();
    auto* clone           = ir::clone(ctx, calledFunction);
    for (auto& bb: *clone) {
        bb.setName(utl::strcat("inline.", bb.name()));
    }
    auto* newGoto = new Goto(ctx, &clone->entry());
    callingBB->insert(call, newGoto);
    auto* returningBB =
        new BasicBlock(ctx, ctx.uniqueName(callingFunction, "inline.return"));
    returningBB->splice(returningBB->begin(),
                        BasicBlock::Iterator(newGoto->next()),
                        callingBB->end());
    callingFunction->insert(callingBB->next(), returningBB);
    /// Add a phi node to `returningBB` the merge all returns from `clone`
    auto* phi =
        new Phi(call->type(), ctx.uniqueName(callingFunction, "inline.phi"));
    returningBB->insert(returningBB->begin(), phi);
    utl::small_vector<PhiMapping> phiArgs;
    /// Replace all parameters with the callers arguments.
    for (auto [param, arg]:
         ranges::views::zip(clone->parameters(), call->arguments()))
    {
        replaceValue(&param, arg);
    }
    /// Traverse the about-to-be-inlined function and replace all returns with
    /// gotos to `returningBB`
    for (auto& bb: *clone) {
        for (auto itr = bb.begin(); itr != bb.end();) {
            auto* ret = dyncast<Return*>(itr.to_address());
            if (!ret) {
                ++itr;
                continue;
            }
            auto* gotoInst = new Goto(ctx, returningBB);
            bb.insert(ret, gotoInst);
            phiArgs.push_back({ &bb, ret->value() });
            itr = bb.erase(BasicBlock::Iterator(ret));
        }
    }
    phi->setArguments(phiArgs);
    returningBB->setPredecessors(
        phiArgs | ranges::views::transform([](auto m) { return m.pred; }) |
        ranges::to<utl::small_vector<BasicBlock*>>);
    /// Now we can replace the call instruction with the phi node and erase it.
    replaceValue(call, phi);
    returningBB->erase(call);
    /// Move basic blocks from the clone into calling function.
    callingFunction->splice(Function::Iterator(returningBB), clone);
    delete clone;
}
