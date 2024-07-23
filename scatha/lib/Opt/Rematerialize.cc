#include "Opt/Passes.h"

#include "Common/Ranges.h"
#include "IR/CFG.h"
#include "IR/Clone.h"
#include "IR/PassRegistry.h"

using namespace scatha;
using namespace ir;
using namespace opt;

SC_REGISTER_PASS(opt::rematerialize, "rematerialize",
                 PassCategory::Experimental, {});

namespace {

struct RematCtx {
    Context& ctx;
    Function& function;

    RematCtx(Context& ctx, Function& function): ctx(ctx), function(function) {}

    bool run();

    bool visitInstruction(Instruction& inst);

    bool visitImpl(Instruction const&) { return false; }
    bool visitImpl(GetElementPointer& gep);
};

} // namespace

bool opt::rematerialize(Context& ctx, Function& function) {
    return RematCtx(ctx, function).run();
}

bool RematCtx::run() {
    bool modified = false;
    auto instructions = function.instructions() | TakeAddress | ToSmallVector<>;
    for (auto* inst: instructions) {
        modified |= visitInstruction(*inst);
    }
    return modified;
}

bool RematCtx::visitInstruction(Instruction& inst) {
    return visit(inst, [this](auto& inst) { return visitImpl(inst); });
}

bool RematCtx::visitImpl(GetElementPointer& gep) {
    bool modified = false;
    auto users = gep.users() | ToSmallVector<>;
    for (Instruction* user: users) {
        if (user->parent() == gep.parent()) {
            continue;
        }
        if (auto* phi = dyncast<Phi*>(user)) {

            continue;
        }
        modified = true;
        auto* gepCopy = [&]() -> Instruction* {
            if (gep.users().size() == 1) {
                return gep.parent()->extract(&gep).release();
            }
            return clone(ctx, &gep).release();
        }();
        user->parent()->insert(user, gepCopy);
        if (&gep != gepCopy) {
            user->updateOperand(&gep, gepCopy);
        }
    }
    return modified;
}
