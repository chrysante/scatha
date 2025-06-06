#include "CodeGen/Passes.h"

#include "CodeGen/InterferenceGraph.h"
#include "CodeGen/TargetInfo.h"
#include "CodeGen/Utility.h"
#include "MIR/CFG.h"
#include "MIR/Instructions.h"

using namespace scatha;
using namespace cg;
using namespace mir;
using namespace ranges::views;

/// For instructions that are three address instructions in the MIR but two
/// address instructions in the VM, we issue copies of the first operand into
/// the destination register and then replace the first operand by the dest
/// register.
static void convertToTwoAddressMode(Function& F) {
    auto instructions =
        F.instructions() |
        Filter<UnaryArithmeticInst, ArithmeticInst, ConversionInst>;
    for (auto& inst: instructions) {
        auto* dest = inst.dest();
        auto* operand = inst.operandAt(0);
        if (dest == operand) {
            continue;
        }
        if (auto* arithmetic = dyncast<ValueArithmeticInst*>(&inst);
            arithmetic && arithmetic->RHS() == dest)
        {
            if (isCommutative(arithmetic->operation())) {
                inst.setOperandAt(0, arithmetic->RHS());
                inst.setOperandAt(1, operand);
                continue;
            }
            auto* tmp = new VirtualRegister();
            F.virtualRegisters().add(tmp);
            auto* copy = new CopyInst(tmp, arithmetic->LHS(),
                                      arithmetic->bytewidth(),
                                      arithmetic->metadata());
            inst.parent()->insert(&inst, copy);
            inst.setOperandAt(1, tmp);
        }
        SC_ASSERT(
            !ranges::contains(inst.operands() | drop(1), dest),
            "The other operands must not contain dest because we clobber dest with a copy before execution the instruction");
        auto* copy =
            new CopyInst(dest, operand, inst.bytewidth(), inst.metadata());
        inst.parent()->insert(&inst, copy);
        inst.setOperandAt(0, dest);
    }
}

static void allocateHardwareRegisters(Function& F, size_t numRegs) {
    SC_ASSERT(
        F.hardwareRegisters().empty(),
        "Must be empty because we are allocating `numRegs` new registers that we expect to be indexed with [0, numRegs)");
    for (size_t i = 0; i < numRegs; ++i)
        F.hardwareRegisters().add(new HardwareRegister());
}

/// Replace all virtual registers with the newly allocated hardware registers
static void replaceVirtRegsWithHardwareRegs(Function& F,
                                            InterferenceGraph const& graph) {
    utl::hashmap<VirtualRegister*, HardwareRegister*> registerMap;
    for (auto* node: graph) {
        auto* vreg = dyncast<VirtualRegister*>(node->reg());
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
}

static void evictCopyInstructions(Function& F) {
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
            if (auto* constant = dyncast<Constant*>(copy->source());
                constant && constant->value() == 0 && copy->bytewidth() > 2)
            {
                auto* selfXor =
                    new ValueArithmeticInst(copy->dest(), copy->dest(),
                                            copy->dest(), copy->bytewidth(),
                                            ArithmeticOperation::XOr,
                                            copy->metadata());
                BB.insert(itr, selfXor);
                itr = BB.erase(itr);
                continue;
            }
            ++itr;
        }
    }
}

/// Erase all instructions that are not critical and don't define live registers
static void evictUnusedInstructions(Function& F) {
    for (auto& BB: F) {
        /// We make a copy because we modify the live set as we traverse the
        /// block, so we know at each instruction which registers are live.
        auto live = BB.liveOut();
        utl::small_vector<Instruction*> toErase;
        for (auto& inst: BB | ranges::views::reverse) {
            auto isLive = [&](auto* reg) { return live.contains(reg); };
            bool canErase = !hasSideEffects(inst) &&
                            !isa<CalleeRegister>(inst.dest()) &&
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
            for (auto* reg: inst.operands() | Filter<Register>) {
                live.insert(reg);
            }
        }
        for (auto* inst: toErase) {
            BB.erase(inst);
        }
    }
}

/// As a last step we allocate callee registers to the upper hardware registers.
/// We first replace all callee registers with new hardware registers
static void allocateCalleeRegisters(Function& F) {
    size_t numRegs = F.hardwareRegisters().size();
    for (auto& calleeReg: F.calleeRegisters()) {
        auto* hReg = new HardwareRegister();
        F.hardwareRegisters().add(hReg);
        calleeReg.replaceWith(hReg);
    }
    /// Then we set the register offset argument of all call instructions
    for (auto& call: F | ranges::views::join | Filter<CallInst>) {
        size_t offset = [&] {
            if (call.isNative()) return numRegs + numRegistersForCallMetadata();
            return numRegs;
        }();
        call.setRegisterOffset(offset);
    }
}

void cg::allocateRegisters(Context&, Function& F) {
    convertToTwoAddressMode(F);
    /// Now we color the interference graph and replace registers
    /// This is were the actual work happens, everything is this file is mostly
    /// auxiliary
    auto graph = InterferenceGraph::compute(F);
    graph.colorize();
    size_t numCols = graph.numColors();
    allocateHardwareRegisters(F, numCols);
    replaceVirtRegsWithHardwareRegs(F, graph);
    /// Then we try to evict some copy instructions.
    evictCopyInstructions(F);
    evictUnusedInstructions(F);
    SC_ASSERT(numCols == F.hardwareRegisters().size(),
              "Make sure we haven't added any more hardware registers");
    allocateCalleeRegisters(F);
    F.setRegisterPhase(RegisterPhase::Hardware);
}
