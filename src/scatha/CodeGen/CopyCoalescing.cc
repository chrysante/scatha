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

    bool evictDefWithExistingValue(Instruction& inst, LiveInterval sourceValue,
                                   LiveInterval destValue);

    utl::hashset<CopyInst*> evictedCopies;
};

} // namespace

void cg::coalesceCopies(Context& ctx, Function& F, CodegenOptions const&) {
    CCContext(ctx, F).run();
}

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
    survive->replaceLiveInterval(surviveValue, merge(surviveValue.reg,
                                                     killValue, surviveValue));
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
    if (evictDefWithExistingValue(inst, *sourceValue, *destValue)) {
        return;
    }
}

static Instruction* getDefInst(Function& F, LiveInterval value) {
    auto FLin = F.linearInstructions();
    auto itr = ranges::lower_bound(FLin, value.begin, ranges::less{},
                                   &Instruction::index);
    if (itr == FLin.end() || (*itr)->index() != value.begin) {
        return nullptr;
    }
    return *itr;
}

bool CCContext::evictDefWithExistingValue(Instruction& inst,
                                          LiveInterval sourceValue,
                                          LiveInterval destValue) {
    if (!isa<CopyInst>(inst)) {
        return false;
    }
    auto* dest = inst.dest();
    auto defs = dest->defs() | ToSmallVector<>;
    ranges::sort(defs, ranges::less{}, &Instruction::index);
    auto instItr = ranges::lower_bound(defs, inst.index(), ranges::less{},
                                       &Instruction::index);
    SC_ASSERT(instItr != defs.end() && *instItr == &inst,
              "This must be our instruction");
    if (instItr == defs.begin()) {
        return false;
    }
    /// The instruction that defines `dest` before we do
    auto* prevDef = *std::prev(instItr);
    /// The value in currently `dest`
    auto existingValue = dest->liveIntervalAt(prevDef->index());
    if (!existingValue) {
        return false;
    }
    auto* sourceCopy = dyncast<CopyInst*>(getDefInst(F, sourceValue));
    /// We traverse the chain of copy instructions and check if we copy the
    /// value into `dest` that already resides in `dest`
    while (sourceCopy) {
        if (sourceCopy->operandAt(0) == dest &&
            existingValue->begin <= sourceCopy->index() &&
            existingValue->end >= sourceCopy->index())
        {
            dest->removeLiveInterval(destValue);
            dest->replaceLiveInterval(*existingValue,
                                      merge(*existingValue, destValue));
            evictedCopies.insert(cast<CopyInst*>(&inst));
            return true;
        }
        auto* sourceReg = dyncast<Register*>(sourceCopy->source());
        if (!sourceReg) {
            return false;
        }
        auto copySource = sourceReg->liveIntervalAt(sourceCopy->index() - 1);
        if (!copySource) {
            return false;
        }
        sourceCopy = dyncast<CopyInst*>(getDefInst(F, *copySource));
    }
    return false;
}
