#include "CodeGen/Passes.h"

#include "CodeGen/InterferenceGraph.h"
#include "CodeGen/Utility.h"
#include "MIR/CFG.h"

using namespace scatha;
using namespace cg;

void cg::allocateRegisters(mir::Function& F) {
    for (auto& inst: F.instructions()) {
        using enum mir::InstCode;
        switch (inst.instcode()) {
        case UnaryArithmetic:
            [[fallthrough]];
        case Arithmetic:
            [[fallthrough]];
        case Conversion: {
            /// For instructions that are three address instructions in the MIR
            /// but two address instructions in the VM, we issue copies of the
            /// first operand into the destination register and then replace the
            /// first operand by the dest register.
            auto* dest = inst.dest();
            auto* operand = inst.operandAt(0);
            SC_ASSERT(dest != operand,
                      "Here we should be still in kind of SSA form");
            auto* copy =
                new mir::Instruction(mir::InstCode::Copy, dest, { operand });
            inst.parent()->insert(&inst, copy);
            inst.setOperandAt(0, dest);
            break;
        }
        default:
            break;
        }
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
        for (auto inst = BB.begin(); inst != BB.end();) {
            if (inst->instcode() != mir::InstCode::Copy) {
                ++inst;
                continue;
            }
            auto* dest = inst->dest();
            auto* source = inst->operandAt(0);
            if (dest == source) {
                inst = BB.erase(inst);
                continue;
            }
            /// We replace copies from constant 0 with self-xor's. This
            /// decreases binary size because two register indices take 2 bytes
            /// to encode vs >2 bytes for the zero literal.
            if (auto* constant = dyncast<mir::Constant*>(source);
                constant && constant->value() == 0 && inst->bytewidth() > 2)
            {
                auto* selfXor =
                    new mir::Instruction(mir::InstCode::Arithmetic,
                                         dest,
                                         { dest, dest },
                                         mir::ArithmeticOperation::XOr,
                                         8);
                BB.insert(inst, selfXor);
                inst = BB.erase(inst);
                continue;
            }
            ++inst;
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
            bool canErase =
                !hasSideEffects(&inst) &&
                !isa_or_null<mir::CalleeRegister>(inst.dest()) &&
                ranges::none_of(inst.destRegisters(), [&](auto* dest) {
                    return live.contains(dest);
                });
            if (canErase) {
                toErase.push_back(&inst);
                continue;
            }
            /// We erase the destination register from the live set because it
            /// is overridden here, except when the instruction is a `cmov`,
            /// because that does not necessarily define the register
            if (inst.instcode() != mir::InstCode::CondCopy) {
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
    for (auto& BB: F) {
        for (auto& inst: BB) {
            if (inst.instcode() != mir::InstCode::Call &&
                inst.instcode() != mir::InstCode::CallExt)
            {
                continue;
            }
            auto callData = inst.instDataAs<mir::CallInstData>();
            callData.regOffset = utl::narrow_cast<uint8_t>(
                numLocalRegs + NumRegsForCallMetadata);
            inst.setInstData(callData);
        }
    }
}
