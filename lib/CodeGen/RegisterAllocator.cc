#include "CodeGen/RegisterAllocator.h"

#include "CodeGen/InterferenceGraph.h"
#include "MIR/CFG.h"

using namespace scatha;
using namespace cg;

static void replaceUsesAndDefsWith(mir::Register* old, mir::Register* repl) {
    auto defs = old->defs() | ranges::to<utl::small_vector<mir::Instruction*>>;
    for (auto* inst: defs) {
        inst->setDest(repl);
    }
    auto uses = old->uses() | ranges::to<utl::small_vector<mir::Instruction*>>;
    for (auto* inst: uses) {
        inst->replaceOperand(old, repl);
    }
}

/// Before allocating registers the MIR is in somewhat of an SSA form, as every
/// instruction assigns a new virtual register. Register allocation assigns
/// actual hardware registers to the virtual registers and thus destroys SSA
/// form.
void cg::allocateRegisters(mir::Function& F) {
    /// For instructions that are three address instructions in the MIR but two
    /// address instructions in the VM, we issue copies of the first operand
    /// into the destination register and than replace the first operand by the
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
    for (auto* node: graph) {
        size_t color = node->color();
        replaceUsesAndDefsWith(node->reg(), F.registerAt(color));
    }

    /// Then we erase self assigning copy instructions.
    for (auto& BB: F) {
        for (auto inst = BB.begin(); inst != BB.end();) {
            if (inst->instcode() != mir::InstCode::Copy ||
                inst->dest() != inst->operandAt(0))
            {
                ++inst;
                continue;
            }
            inst = BB.erase(inst);
        }
    }
}
