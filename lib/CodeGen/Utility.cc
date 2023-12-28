#include "CodeGen/Utility.h"

#include <range/v3/algorithm.hpp>
#include <range/v3/view.hpp>

#include "MIR/CFG.h"
#include "MIR/Instructions.h"
#include "MIR/LiveInterval.h"

using namespace scatha;
using namespace cg;
using namespace mir;
using namespace ranges::views;

bool cg::hasSideEffects(Instruction const& inst) {
    return isa<StoreInst>(inst) || isa<CallInst>(inst) ||
           isa<ReturnInst>(inst) || isa<JumpBase>(inst) ||
           isa<CompareInst>(inst) || isa<TestInst>(inst);
}

static LiveInterval computeLiveInterval(Function& F,
                                        BasicBlock& BB,
                                        Register* reg,
                                        int32_t begin) {
    int32_t end = begin;
    int32_t first =
        std::max(begin + 1, static_cast<int32_t>(BB.front().index()));
    /// We traverse the function starting from the program point one past the
    /// begin
    for (auto progPoint: F.programPoints() | drop(first)) {
        /// If the basic block ends the live range ends
        if (std::holds_alternative<BasicBlock*>(progPoint)) {
            break;
        }
        auto* inst = std::get<Instruction*>(progPoint);
        if (inst->parent() != &BB) {
            break;
        }
        /// Live range extends to at least the last use of the register in this
        /// block
        if (ranges::contains(inst->operands(), reg)) {
            end = utl::narrow_cast<int32_t>(inst->index());
        }
        /// cmov instruction also "read" the dest value because they clobber
        /// only conditionally
        if (isa<CondCopyInst>(inst) && inst->dest() == reg) {
            end = utl::narrow_cast<int32_t>(inst->index());
        }
        /// Live range ends at the last use before a def (except for cmovs which
        /// clobber only conditionally)
        if (ranges::contains(inst->destRegisters(), reg) &&
            !isa<CondCopyInst>(inst)) {
            return { begin, end };
        }
        /// Calls clobber all callee registers
        if (isa<CallInst>(inst) && isa<CalleeRegister>(reg)) {
            return { begin, end };
        }
    }
    /// If the register is live-out the live range extends to the end of the
    /// block
    if (BB.liveOut().contains(reg)) {
        return { begin, BB.back().index() + 1 };
    }
    /// If the register is not live-out the live range extends to the last use
    return { begin, end };
}

void cg::computeLiveRange(Function& F, Register& reg) {
    std::vector<LiveInterval> liveRange;
    for (auto& BB: F) {
        if (BB.liveIn().contains(&reg)) {
            liveRange.push_back(computeLiveInterval(F, BB, &reg, BB.index()));
        }
    }
    if (isa<CalleeRegister>(reg)) {
        for (auto* call: F.linearInstructions() | Filter<CallInst>) {
            liveRange.push_back(
                computeLiveInterval(F, *call->parent(), &reg, call->index()));
        }
    }
    for (auto* def: reg.defs()) {
        if (isa<CondCopyInst>(def)) {
            continue;
        }
        liveRange.push_back(
            computeLiveInterval(F, *def->parent(), &reg, def->index()));
    }
    ranges::sort(liveRange);
    reg.setLiveRange(std::move(liveRange));
}
