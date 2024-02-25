#include "CodeGen/Passes.h"

#include "MIR/CFG.h"
#include "MIR/Instructions.h"

using namespace scatha;
using namespace cg;
using namespace mir;
using namespace ranges::views;

static bool isLiveIn(BasicBlock const& BB, LiveInterval I) {
    return BB.index() == I.begin;
}

static bool isLiveOut(BasicBlock const& BB, LiveInterval I) {
    return BB.back().index() + 1 == I.end;
}

namespace {

struct CCContext {
    Context& ctx;
    Function& F;

    CCContext(Context& ctx, Function& F): ctx(ctx), F(F) {}

    void run();

    void visitInst(Instruction& inst);

    void evictIfCopy(Instruction& inst) {
        auto* copy = dyncast<CopyInst*>(&inst);
        if (copy) {
            evictedCopies.insert(copy);
        }
    }

    utl::hashset<CopyInst*> evictedCopies;
};

} // namespace

void cg::coalesceCopies(Context& ctx, Function& F) { CCContext(ctx, F).run(); }

void CCContext::run() {
    for (auto* inst: F.linearInstructions()) {
        visitInst(*inst);
    }
    for (auto* copy: evictedCopies) {
        copy->parent()->erase(copy);
    }
}

///
static bool mayMoveSource(BasicBlock const& BB, Register* reg,
                          LiveInterval value) {
    return !isa<CalleeRegister>(reg) && !isLiveIn(BB, value);
}

///
static bool mayMoveDest(BasicBlock const& BB, Register* reg,
                        LiveInterval value) {
    return !isa<CalleeRegister>(reg) && !isLiveOut(BB, value);
}

///
static void coalesce(BasicBlock& BB, Register* survive,
                     LiveInterval surviveValue, Register* kill,
                     LiveInterval killValue) {
    SC_ASSERT(!overlaps(surviveValue, killValue),
              "Can't coalesce overlapping values");
    auto instRange = BB | drop_while([&](auto& inst) {
        return inst.index() < killValue.begin;
    }) | take_while([&](auto& inst) { return inst.index() <= killValue.end; });
    for (auto& inst: instRange) {
        if (inst.index() != killValue.begin) {
            inst.replaceOperand(kill, survive);
        }
        if (inst.index() != killValue.end && inst.dest() == kill) {
            inst.setDest(survive);
        }
    }
    kill->removeLiveInterval(killValue);
    survive->replaceLiveInterval(surviveValue, merge(killValue, surviveValue));
}

void CCContext::visitInst(Instruction& inst) {
    if (!isa<CopyInst>(inst) && !isa<ArithmeticInst>(inst) &&
        !isa<UnaryArithmeticInst>(inst) && !isa<ConversionInst>(inst))
    {
        return;
    }
    auto& BB = *inst.parent();
    auto* source = dyncast<Register*>(inst.operandAt(0));
    if (!source) {
        return;
    }
    auto* dest = inst.dest();
    if (source == dest) {
        return;
    }
    auto sourceValue = source->liveIntervalAt(inst.index() - 1);
    /// If we would assert here this fails if blocks have no predecessors. This
    /// should not happen in practice but let's be conservative here
    if (!sourceValue) {
        return;
    }
    auto destValue = dest->liveIntervalAt(inst.index());
    /// Unlike source dest may not be used and thus the live range ends at the
    /// definition
    if (!destValue) {
        return;
    }
    /// If the dest value is not a callee register and not live-out, we can
    /// assign the dest value to another register.
    if (mayMoveDest(BB, dest, *destValue)) {
        /// If the dest value does not overlap with any values in source we can
        /// merge the dest value into the source register
        if (rangeOverlap(source->liveRange(), *destValue).empty()) {
            /// Merge dest into source
            coalesce(BB, source, *sourceValue, dest, *destValue);
            evictIfCopy(inst);
            return;
        }
        return;
    }
    /// If the source value is fixed or live-in, we can't evict this copy
    if (!mayMoveSource(BB, source, *sourceValue)) {
        return;
    }
    /// If the source value does not overlap with any values in dest we can
    /// merge the source value into the dest register
    if (rangeOverlap(dest->liveRange(), *sourceValue).empty()) {
        coalesce(BB, dest, *destValue, source, *sourceValue);
        evictIfCopy(inst);
        return;
    }
}
