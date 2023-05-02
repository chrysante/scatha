#include "CodeGen/Passes.h"

#include <utl/hashtable.hpp>

#include "MIR/CFG.h"

using namespace scatha;
using namespace cg;
using namespace mir;

static bool isCritical(mir::Instruction const* inst) {
    switch (inst->instcode()) {
    case InstCode::Store:
    case InstCode::Call:
    case InstCode::CallExt:
    case InstCode::Return:
    case InstCode::Set:
        return true;
    default:
        return false;
    }
}

namespace {

struct DCEContext {
    DCEContext(mir::Function& F): F(F) {}

    bool run();

    void mark();

    void visitInstruction(mir::Instruction* inst);

    void sweep();

    mir::Function& F;
    utl::hashset<mir::Instruction*> deadInstructions;
};

} // namespace

bool cg::deadCodeElim(mir::Function& F) {
    DCEContext ctx(F);
    return ctx.run();
}

bool DCEContext::run() {
    mark();
    sweep();
    return !deadInstructions.empty();
}

void DCEContext::mark() {
    for (auto& BB: F) {
        for (auto& inst: BB) {
            visitInstruction(&inst);
        }
    }
}

void DCEContext::visitInstruction(mir::Instruction* inst) {
    if (isCritical(inst)) {
        return;
    }
    if (deadInstructions.contains(inst)) {
        return;
    }
    auto* dest = dyncast_or_null<SSARegister*>(inst->dest());
    if (!dest) {
        return;
    }
    if (!dest->uses().empty()) {
        return;
    }
    deadInstructions.insert(inst);
    auto ssaOps = inst->operands() |
                  ranges::views::filter([](mir::Value const* value) {
                      return value && isa<mir::SSARegister>(value);
                  }) |
                  ranges::views::transform([](mir::Value* value) {
                      return cast<mir::SSARegister*>(value);
                  }) |
                  ranges::to<utl::small_vector<mir::SSARegister*>>;
    inst->clearOperands();
    for (auto* reg: ssaOps) {
        if (reg->def()) { /// `reg->def()` could be null if `reg` is a parameter
            visitInstruction(reg->def());
        }
    }
}

void DCEContext::sweep() {
    for (auto& BB: F) {
        for (auto* inst: deadInstructions) {
            auto* dest = inst->dest();
            BB.removeLiveIn(dest);
            BB.removeLiveOut(dest);
        }
    }
    for (auto* inst: deadInstructions) {
        inst->parent()->erase(inst);
    }
}
