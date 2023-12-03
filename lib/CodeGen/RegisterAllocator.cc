#include "CodeGen/Passes.h"

#include "CodeGen/InterferenceGraph.h"
#include "CodeGen/Utility.h"
#include "MIR/CFG.h"
#include "MIR/Instructions.h"

using namespace scatha;
using namespace cg;
using namespace mir;

void cg::allocateRegisters(mir::Function& F) {
    /// For instructions that are three address instructions in the MIR
    /// but two address instructions in the VM, we issue copies of the
    /// first operand into the destination register and then replace the
    /// first operand by the dest register.
    for (auto& inst:
         F.instructions() |
             Filter<UnaryArithmeticInst, ArithmeticInst, ConversionInst>)
    {
        auto* dest = inst.dest();
        auto* operand = inst.operandAt(0);
        SC_ASSERT(dest != operand,
                  "Here we should be still in kind of SSA form");
        auto* copy =
            new mir::CopyInst(dest, operand, inst.bytewidth(), inst.metadata());
        inst.parent()->insert(&inst, copy);
        inst.setOperandAt(0, dest);
    }
    /// Now we color the interference graph and replace registers
    /// This is were the actual work happens, everything is this file is mostly
    /// auxiliary
    auto graph = InterferenceGraph::compute(F);
    graph.colorize();
    size_t const numCols = graph.numColors();
    SC_ASSERT(F.hardwareRegisters().empty(),
              "Must be empty because we are allocating `numCols` new registers "
              "that we expect to be indexed with [0, numCols)");
    /// Allocate hardware registers
    for (size_t i = 0; i < numCols; ++i) {
        F.hardwareRegisters().add(new mir::HardwareRegister());
    }
    /// Replace all virtual registers with the newly allocated hardware
    /// registers
    utl::hashmap<mir::VirtualRegister*, mir::HardwareRegister*> registerMap;
    for (auto* node: graph) {
        auto* vreg = dyncast<mir::VirtualRegister*>(node->reg());
        if (!vreg) {
            continue;
        }
        auto* hreg = F.hardwareRegisters().at(node->color());
        vreg->replaceWith(hreg);
        registerMap[vreg] = hreg;
    }
    /// Update live sets with new registers
    /// \Note this code is copied from `mapSSAToVirtualRegisters()`, we could
    /// abstract this
    for (auto& BB: F) {
        for (auto [vreg, hreg]: registerMap) {
            if (BB.isLiveIn(vreg)) {
                BB.addLiveIn(hreg);
            }
            BB.removeLiveIn(vreg);
            if (BB.isLiveOut(vreg)) {
                BB.addLiveOut(hreg);
            }
            BB.removeLiveOut(vreg);
        }
    }
    /// Then we try to evict some copy instructions.
    for (auto& BB: F) {
        for (auto itr = BB.begin(); itr != BB.end();) {
            auto* copy = dyncast<CopyInst*>(itr.to_address());
            if (!copy) {
                ++itr;
                continue;
            }
            if (copy->dest() == copy->source()) {
                itr = BB.erase(itr);
                continue;
            }
            /// We replace copies from constant 0 with self-xor's. This
            /// decreases binary size because two register indices take 2 bytes
            /// to encode vs >2 bytes for the zero literal.
            if (auto* constant = dyncast<mir::Constant*>(copy->source());
                constant && constant->value() == 0 && copy->bytewidth() > 2)
            {
                auto* selfXor =
                    new mir::ValueArithmeticInst(copy->dest(),
                                                 copy->dest(),
                                                 copy->dest(),
                                                 copy->bytewidth(),
                                                 mir::ArithmeticOperation::XOr,
                                                 copy->metadata());
                BB.insert(itr, selfXor);
                itr = BB.erase(itr);
                continue;
            }
            ++itr;
        }
    }
    /// We erase all instructions that are not critical and don't define live
    /// registers
    for (auto& BB: F) {
        /// We make a copy because we modify the live set as we traverse the
        /// block, so we know at each instruction which registers are live.
        auto live = BB.liveOut();
        utl::small_vector<mir::Instruction*> toErase;
        for (auto& inst: BB | ranges::views::reverse) {
            auto isLive = [&](auto* reg) { return live.contains(reg); };
            bool canErase = !hasSideEffects(&inst) &&
                            !isa_or_null<mir::CalleeRegister>(inst.dest()) &&
                            ranges::none_of(inst.destRegisters(), isLive);
            if (canErase) {
                toErase.push_back(&inst);
                continue;
            }
            /// We erase the destination register from the live set because it
            /// is overridden here, except when the instruction is a `cmov`,
            /// because that does not necessarily define the register
            if (!isa<CondCopyInst>(inst)) {
                for (auto* reg: inst.destRegisters()) {
                    live.erase(reg);
                }
            }
            for (auto* reg: inst.operands() | Filter<mir::Register>) {
                live.insert(reg);
            }
        }
        for (auto* inst: toErase) {
            BB.erase(inst);
        }
    }
    /// Now as a last step we allocate callee registers to the upper hardware
    /// registers.
    /// First we reserve some registers for call metadata
    size_t const numLocalRegs = F.hardwareRegisters().size();
    static constexpr size_t NumRegsForCallMetadata = 3;
    for (size_t i = 0; i < NumRegsForCallMetadata; ++i) {
        F.hardwareRegisters().add(new mir::HardwareRegister());
    }
    /// Then we replace all callee registers with new hardware registers
    for (auto& calleeReg: F.calleeRegisters()) {
        auto* hReg = new mir::HardwareRegister();
        F.hardwareRegisters().add(hReg);
        calleeReg.replaceWith(hReg);
    }
    for (auto& call: F | ranges::views::join | Filter<CallBase>) {
        call.setRegisterOffset(numLocalRegs + NumRegsForCallMetadata);
    }
}
