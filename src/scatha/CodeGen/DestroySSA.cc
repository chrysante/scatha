#include "CodeGen/Passes.h"

#include <utl/hashtable.hpp>
#include <utl/strcat.hpp>

#include "CodeGen/TargetInfo.h"
#include "CodeGen/Utility.h"
#include "MIR/CFG.h"
#include "MIR/Instructions.h"
#include "MIR/Register.h"

using namespace scatha;
using namespace cg;
using namespace mir;
using namespace ranges::views;

static bool isTailCall(CallInst const& call) {
    /// Make sure this is not a call to a foreign function because we can't jump
    /// to those.
    /// As of right now we don't suppport TCO for indirect calls either. This
    /// shouldn't be hard to fix, we only need to implement indirect jump
    /// instructions
    if (auto* cv = dyncast<CallValueInst const*>(&call);
        !cv || !isa<Function>(cv->callee()))
    {
        return false;
    }
    auto* ret = dyncast<ReturnInst const*>(call.next());
    if (!ret) {
        return false;
    }
    if (ret->operands().size() != call.numDests()) {
        return false;
    }
    if (!call.dest()) {
        return true;
    }
    return ret->operands().front() == call.dest();
}

static void mapSSAToVirtualRegisters(Function& F) {
    utl::hashmap<SSARegister*, VirtualRegister*> registerMap;
    /// Create virtual registers for all SSA registers.
    size_t const numRegs =
        F.ssaRegisters().size() -
        std::min(F.ssaRegisters().size(), F.numReturnValueRegisters());
    for (size_t i = 0; i < numRegs; ++i) {
        F.virtualRegisters().add(new VirtualRegister());
    }
    /// Replace all SSA registers with 'virtual' registers
    size_t numFixed =
        std::max(F.numArgumentRegisters(), F.numReturnValueRegisters());
    for (size_t i = 0, end = F.ssaRegisters().size(); i < end; ++i) {
        auto* ssaReg = F.ssaRegisters().at(i);
        auto* vReg = F.virtualRegisters().at(i);
        if (i < numFixed) {
            vReg->setFixed();
        }
        ssaReg->replaceWith(vReg);
        registerMap[ssaReg] = vReg;
    }
    /// Update live sets with new registers
    for (auto& BB: F) {
        for (auto [ssaReg, vReg]: registerMap) {
            if (BB.isLiveIn(ssaReg)) {
                BB.addLiveIn(vReg);
            }
            BB.removeLiveIn(ssaReg);
            if (BB.isLiveOut(ssaReg)) {
                BB.addLiveOut(vReg);
            }
            BB.removeLiveOut(ssaReg);
        }
    }
}

static BasicBlock::Iterator destroyTailCall(Function& F, BasicBlock& BB,
                                            CallInst& call,
                                            BasicBlock::Iterator itr) {
    size_t const numArgs = call.operands().size() - 1;
    /// We allocate bottom registers for the arguments
    for (size_t i = F.virtualRegisters().size(); i < numArgs; ++i) {
        F.virtualRegisters().add(new VirtualRegister());
    }
    /// Copy arguments into bottom registers.
    auto insertPoint = itr;
    auto tmpCopyInsertPoint = std::prev(insertPoint);
    for (auto [index, dest, arg]:
         zip(iota(0), F.virtualRegisters(), call.arguments()))
    {
        dest.setFixed();
        /// We mark the arguments registers live out since they need to be
        /// preserved for the called function
        BB.addLiveOut(&dest);
        if (&dest == arg) {
            continue;
        }
        /// If our argument is in one of the dest registers that has already
        /// been overwritten, we copy the argument to a temporary location
        /// _before_ the argument copies and then copy into the dest register
        /// from the temporary
        if (ranges::contains(F.virtualRegisters() | TakeAddress | take(index),
                             arg))
        {
            auto* tmp = new VirtualRegister();
            F.virtualRegisters().add(tmp);
            BB.insert(std::next(tmpCopyInsertPoint),
                      new CopyInst(tmp, arg, 8, call.metadata()));
            BB.insert(insertPoint,
                      new CopyInst(&dest, tmp, 8, call.metadata()));
        }
        /// Otherwise we copy directly to the dest register
        else {
            auto* copy = new CopyInst(&dest, arg, 8, call.metadata());
            BB.insert(insertPoint, copy);
        }
    }
    auto& ret = *call.next();
    SC_ASSERT(isa<ReturnInst>(ret), "We are not a tailcall");
    std::advance(itr, 2);
    // clang-format off
    auto* jump = SC_MATCH (call) {
        [&](CallValueInst& call) {
            return new JumpInst(call.callee(), call.metadata());
        },
        [&](CallMemoryInst&) -> JumpInst* {
            SC_UNIMPLEMENTED();
        }
    }; // clang-format on
    BB.erase(&call);
    BB.erase(&ret);
    BB.insert(itr, jump);
    SC_ASSERT(itr == BB.end(), "");
    return itr;
}

static BasicBlock::Iterator destroy(Function& F, BasicBlock& BB, CallInst& call,
                                    BasicBlock::Iterator const callItr) {
    if (isTailCall(call)) {
        return destroyTailCall(F, BB, call, callItr);
    }
    size_t numMDRegs = call.isNative() ? numRegistersForCallMetadata() : 0;
    size_t numCalleeRegs =
        numMDRegs + std::max(call.arguments().size(), call.numDests());
    /// Allocate additional callee registers if not enough present.
    for (size_t i = F.calleeRegisters().size(); i < numCalleeRegs; ++i) {
        auto* reg = new CalleeRegister();
        F.calleeRegisters().add(reg);
    }
    /// Copy arguments into callee registers.
    auto newArguments = call.operands() | take(call.numCalleeOperands()) |
                        ToSmallVector<>;
    newArguments.reserve(call.operands().size());
    auto dest =
        std::next(F.calleeRegisters().begin(), static_cast<ssize_t>(numMDRegs));
    for (auto argItr = call.arguments().begin(), end = call.arguments().end();
         argItr != end; ++dest, ++argItr)
    {
        Value* arg = *argItr;
        CalleeRegister* destReg = dest.to_address();
        auto* copy = new CopyInst(destReg, arg, 8, call.metadata());
        BB.insert(&call, copy);
        newArguments.push_back(destReg);
        destReg->addUser(&call);
    }
    /// Call instructions define registers as long as we work with SSA
    /// registers. From here on we explicitly copy the arguments out of the
    /// register space of the callee.
    SC_ASSERT(F.calleeRegisters().size() >= call.numDests(), "");
    auto destAndCalleeRegs =
        zip(call.destRegisters(), F.calleeRegisters() | drop(numMDRegs));
    for (auto [dest, calleeReg]: destAndCalleeRegs) {
        auto* copy = new CopyInst(dest, &calleeReg, 8, call.metadata());
        BB.insert(std::next(callItr), copy);
    }
    /// We don't define registers anymore, see comment above.
    call.clearDest();
    call.setOperands(newArguments);
    return std::next(callItr);
}

static BasicBlock::Iterator destroy(Function& F, BasicBlock& BB,
                                    ReturnInst& ret, BasicBlock::Iterator itr) {
    for (auto [arg, dest]: zip(ret.operands(), F.virtualReturnValueRegisters()))
    {
        auto* copy = new CopyInst(dest, arg, 8, ret.metadata());
        BB.insert(itr, copy);
        if (auto* argReg = dyncast<Register*>(arg)) {
            BB.removeLiveOut(argReg);
        }
        BB.addLiveOut(dest);
    }
    ret.clearOperands();
    return ++itr;
}

[[maybe_unused]] static void splitEdge(BasicBlock* pred, BasicBlock* succ) {
    auto* F = succ->parent();
    auto* splitBlock =
        new BasicBlock(utl::strcat(pred->name(), "->", succ->name()));
    F->insert(succ, splitBlock);
    splitBlock->pushBack(new JumpInst(succ, {}));
    for (auto& inst: *pred | reverse) {
        if (!isa<TerminatorInst>(inst)) {
            break;
        }
        inst.replaceOperand(succ, splitBlock);
    }
    splitBlock->addSuccessor(succ);
    splitBlock->addPredecessor(pred);
    pred->replaceSuccessor(succ, splitBlock);
    succ->replacePredecessor(pred, splitBlock);
    splitBlock->setLiveIn(pred->liveOut());
    splitBlock->setLiveOut(pred->liveOut());
}

static BasicBlock::Iterator destroy(Function& F, BasicBlock& BB, PhiInst& phi,
                                    BasicBlock::Iterator itr) {
    auto* dest = phi.dest();
    bool const needTmp = ranges::any_of(BB.predecessors(), [&](auto* pred) {
        return isCriticalEdge(pred, &BB);
    }) || ranges::any_of(dest->uses(), [&](Instruction* user) {
        return isa<PhiInst>(user) && user->parent() == phi.parent();
    });
    if (needTmp) {
        auto* tmp = new VirtualRegister();
        dest = tmp;
        F.virtualRegisters().add(tmp);
        BB.insert(&phi, new CopyInst(phi.dest(), tmp, phi.bytewidth(),
                                     phi.metadata()));
        BB.addLiveIn(tmp);
        BB.removeLiveIn(phi.dest());
    }
    for (auto [pred, arg]: zip(BB.predecessors(), phi.operands())) {
        auto before = pred->end();
        while (before != pred->begin()) {
            auto p = std::prev(before);
            if (!isa<TerminatorInst>(*p)) {
                break;
            }
            before = p;
        }
        pred->insert(before,
                     new CopyInst(dest, arg, phi.bytewidth(), phi.metadata()));
        /// Update live sets to honor inserted copy
        if (auto* argReg = dyncast<Register*>(arg)) {
            /// True if this argument appears only once in the phi arguments.
            /// We only remove live out values if the argument is unique. This
            /// is a bit conservative but multiple phi arguments are perhaps a
            /// problem that the optimizer has to solve
            bool argUnique = ranges::count(phi.operands(), arg) == 1;
            if (argUnique) {
                bool argDead = !BB.isLiveIn(argReg);
                /// If the uses of the phi argument are all in the predecessor
                /// or are this phi
                for (auto* use: argReg->uses()) {
                    argDead &= use == &phi || use->parent() == pred;
                }
                if (argDead) {
                    pred->removeLiveOut(argReg);
                }
            }
        }
        pred->addLiveOut(dest);
    }
    return BB.erase(itr);
}

static BasicBlock::Iterator destroy(Function&, BasicBlock& BB,
                                    SelectInst& select,
                                    BasicBlock::Iterator itr) {
    auto* copy = new CopyInst(select.dest(), select.thenValue(),
                              select.bytewidth(), select.metadata());
    auto* cndCopy =
        new CondCopyInst(select.dest(), select.elseValue(), select.bytewidth(),
                         inverse(select.condition()), select.metadata());
    BB.insert(itr, copy);
    BB.insert(itr, cndCopy);
    return BB.erase(itr);
}

/// Base case
static BasicBlock::Iterator destroy(Function&, BasicBlock&, Instruction&,
                                    BasicBlock::Iterator itr) {
    return std::next(itr);
}

/// We mark every call instruction as a definition of every callee register,
/// because calls clobber all callee registers. We need to do this here because
/// we allocate callee registers lazily above
static void clobberCalleeRegisters(Function& F) {
    for (auto& reg: F.calleeRegisters()) {
        for (auto& inst: F.instructions() | Filter<CallInst>) {
            reg.addDef(&inst);
        }
    }
}

void cg::destroySSA(mir::Context&, Function& F) {
    mapSSAToVirtualRegisters(F);
    for (auto& BB: F) {
        for (auto itr = BB.begin(); itr != BB.end();) {
            itr = visit(*itr,
                        [&](auto& inst) { return destroy(F, BB, inst, itr); });
        }
    }
    clobberCalleeRegisters(F);
    F.setRegisterPhase(RegisterPhase::Virtual);
    F.linearize();
    for (auto& reg: F.virtAndCalleeRegs()) {
        computeLiveRange(F, reg);
    }
}
