#include "CodeGen/RegisterAllocator.h"

#include "CodeGen/InterferenceGraph.h"
#include "MIR/CFG.h"

using namespace scatha;
using namespace cg;

void cg::allocateRegisters(mir::Function& F) {
    /// For instructions that are three address instructions in the MIR but two
    /// address instructions in the VM, we issue copies of the first operand
    /// into the destination register and then replace the first operand by the
    /// dest register.
    for (auto& BB: F) {
        for (auto& inst: BB) {
            if (inst.instcode() != mir::InstCode::UnaryArithmetic &&
                inst.instcode() != mir::InstCode::Arithmetic &&
                inst.instcode() != mir::InstCode::Conversion)
            {
                continue;
            }
            auto* dest    = inst.dest();
            auto* operand = inst.operandAt(0);
            SC_ASSERT(dest != operand,
                      "Here we should be still in kind of SSA form");
            auto* copy =
                new mir::Instruction(mir::InstCode::Copy, dest, { operand });
            BB.insert(&inst, copy);
            inst.setOperandAt(0, dest);
        }
    }
    /// Now we color the interference graph and replace registers
    auto graph = InterferenceGraph::compute(F);
    graph.colorize();
    size_t const numCols = graph.numColors();
    SC_ASSERT(F.hardwareRegisters().empty(),
              "Must be empty because we are allocating `numCols` new registers "
              "that we expect to be indexed with [0, numCols)");
    for (size_t i = 0; i < numCols; ++i) {
        F.hardwareRegisters().add(new mir::HardwareRegister());
    }
    for (auto* node: graph) {
        size_t const color = node->color();
        node->reg()->replaceWith(F.hardwareRegisters().at(color));
    }
    /// Then we erase self assigning copy instructions.
    for (auto& BB: F) {
        for (auto inst = BB.begin(); inst != BB.end();) {
            if (inst->instcode() != mir::InstCode::Copy) {
                ++inst;
                continue;
            }
            auto* dest     = inst->dest();
            auto* source = inst->operandAt(0);
            if (auto* constant = dyncast<mir::Constant*>(source);
                constant && constant->value() == 0)
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
            if (dest == source) {
                inst = BB.erase(inst);
                continue;
            }
            ++inst;
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
            auto callData      = inst.instDataAs<mir::CallInstData>();
            callData.regOffset = utl::narrow_cast<uint8_t>(
                numLocalRegs + NumRegsForCallMetadata);
            inst.setInstData(callData);
        }
    }
}
