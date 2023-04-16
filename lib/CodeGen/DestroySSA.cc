#include "CodeGen/Passes.h"

#include <utl/hashtable.hpp>

#include "MIR/CFG.h"
#include "MIR/Register.h"

using namespace scatha;
using namespace cg;

static bool isTailCall(mir::Instruction const& call) {
    auto& ret = *call.next();
    if (ret.instcode() != mir::InstCode::Return) {
        return false;
    }
    if (ret.operands().size() != call.numDests()) {
        return false;
    }
    if (!call.dest()) {
        return true;
    }
    return ret.operands().front() == call.dest();
}

static bool isCriticalEdge(mir::BasicBlock const* from,
                           mir::BasicBlock const* to) {
    return from->successors().size() > 1 && to->predecessors().size() > 1;
}

static void mapSSAToVirtualRegisters(mir::Function& F) {
    utl::hashmap<mir::SSARegister*, mir::VirtualRegister*> registerMap;
    /// Create virtual registers for all SSA registers.
    size_t const numRegs =
        F.ssaRegisters().size() -
        std::min(F.ssaRegisters().size(), F.numReturnValueRegisters());
    for (size_t i = 0; i < numRegs; ++i) {
        F.virtualRegisters().add(new mir::VirtualRegister());
    }
    /// Replace all SSA registers with 'virtual' registers
    /// Argument registers:
    for (size_t i = 0, end = F.numArgumentRegisters(); i < end; ++i) {
        auto* ssaReg = F.ssaRegisters().at(i);
        auto* vReg   = F.virtualRegisters().at(i);
        vReg->setFixed();
        ssaReg->replaceWith(vReg);
        registerMap[ssaReg] = vReg;
    }
    /// All other registers:
    for (size_t i = F.numArgumentRegisters(), end = F.ssaRegisters().size();
         i < end;
         ++i)
    {
        auto* ssaReg = F.ssaRegisters().at(i);
        auto* vReg   = F.virtualRegisters().at(i);
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

static mir::BasicBlock::Iterator destroySSACall(mir::Function& F,
                                                mir::BasicBlock& BB,
                                                mir::BasicBlock::Iterator itr) {
    auto& call           = *itr;
    auto argBegin        = call.operands().begin();
    size_t numCalleeRegs = call.operands().size();
    bool const isExt     = call.instcode() == mir::InstCode::CallExt;
    mir::Value* callee   = nullptr;
    if (!isExt) {
        callee = *argBegin;
        ++argBegin;
        --numCalleeRegs;
    }
    numCalleeRegs = std::max(numCalleeRegs, call.numDests());
    /// Allocate additional callee registers if not enough present.
    for (size_t i = F.calleeRegisters().size(); i < numCalleeRegs; ++i) {
        auto* reg = new mir::CalleeRegister();
        F.calleeRegisters().add(reg);
    }
    /// Copy arguments into callee registers.
    for (auto dest = F.calleeRegisters().begin();
         argBegin != call.operands().end();
         ++dest, ++argBegin)
    {
        auto* copy = new mir::Instruction(mir::InstCode::Copy,
                                          dest.to_address(),
                                          { *argBegin });
        BB.insert(&call, copy);
    }
    ++itr;
    /// Copy arguments out of callee registers
    if (auto* dest = call.dest()) {
        auto calleeReg = F.calleeRegisters().begin().to_address();
        SC_ASSERT(F.calleeRegisters().size() >= call.numDests(), "");
        for (size_t i = 0; i < call.numDests();
             ++i, dest = dest->next(), calleeReg = calleeReg->next())
        {
            auto* copy =
                new mir::Instruction(mir::InstCode::Copy, dest, { calleeReg });
            BB.insert(itr, copy);
        }
    }
    call.clearOperands();
    if (!isExt) {
        call.setOperands({ callee });
    }
    return itr;
}

static mir::BasicBlock::Iterator destroyReturn(mir::Function& F,
                                               mir::BasicBlock& BB,
                                               mir::BasicBlock::Iterator itr) {
    auto& ret = *itr;
    auto dest = F.virtualReturnValueRegisters().begin();
    for (auto* arg: ret.operands()) {
        auto* copy =
            new mir::Instruction(mir::InstCode::Copy, *dest++, { arg });
        BB.insert(itr, copy);
    }
    ret.clearOperands();
    return ++itr;
}

static mir::BasicBlock::Iterator destroySSATailCall(
    mir::Function& F, mir::BasicBlock& BB, mir::BasicBlock::Iterator itr) {
    SC_ASSERT(itr->instcode() != mir::InstCode::CallExt,
              "Can't tail call ext functions");
    auto& call           = *itr;
    mir::Value* callee   = call.operandAt(0);
    auto argBegin        = std::next(call.operands().begin());
    size_t const numArgs = call.operands().size() - 1;
    /// We need to copy our arguments to temporary registers before copying them
    /// into our bottom registers to make sure we don't overwrite anything that
    /// we still need to copy.
    utl::small_vector<mir::VirtualRegister*, 8> tmpRegs(numArgs);
    /// Allocate additional temporary registers.
    for (size_t i = 0; i < numArgs; ++i, ++argBegin) {
        auto* tmp = new mir::VirtualRegister();
        F.virtualRegisters().add(tmp);
        tmpRegs[i] = tmp;
        auto* copy =
            new mir::Instruction(mir::InstCode::Copy, tmp, { *argBegin });
        BB.insert(&call, copy);
    }
    /// Copy arguments into bottom registers.
    for (auto dest = F.virtualRegisters().begin(); auto* tmp: tmpRegs) {
        auto* copy = new mir::Instruction(mir::InstCode::Copy,
                                          dest.to_address(),
                                          { tmp });
        BB.insert(&call, copy);
        dest->setFixed();
        ++dest;
    }
    auto& ret = *call.next();
    SC_ASSERT(ret.instcode() == mir::InstCode::Return, "We are not a tailcall");
    std::advance(itr, 2);
    BB.erase(&call);
    BB.erase(&ret);
    auto* jump = new mir::Instruction(mir::InstCode::Jump, nullptr, { callee });
    BB.insert(itr, jump);
    SC_ASSERT(itr == BB.end(), "");
    return itr;
}

static mir::BasicBlock::Iterator destroyPhi(mir::Function& F,
                                            mir::BasicBlock& BB,
                                            mir::BasicBlock::Iterator itr) {
    bool const needTmp = ranges::any_of(BB.predecessors(), [&](auto* pred) {
        return isCriticalEdge(pred, &BB);
    });
    auto& phi          = *itr;
    auto* dest         = phi.dest();
    if (needTmp) {
        auto* tmp = new mir::VirtualRegister();
        dest      = tmp;
        F.virtualRegisters().add(tmp);
        BB.insert(&phi,
                  new mir::Instruction(mir::InstCode::Copy,
                                       phi.dest(),
                                       { tmp },
                                       0,
                                       phi.bytewidth()));
        BB.addLiveIn(tmp);
        BB.removeLiveIn(phi.dest());
    }
    for (auto [pred, arg]:
         ranges::views::zip(BB.predecessors(), phi.operands()))
    {
        auto before = pred->end();
        while (true) {
            auto p = std::prev(before);
            if (!isTerminator(p->instcode())) {
                break;
            }
            before = p;
        }
        pred->insert(before,
                     new mir::Instruction(mir::InstCode::Copy,
                                          dest,
                                          { arg },
                                          0,
                                          phi.bytewidth()));
        /// Update live sets to honor inserted copy.
        if (auto* argReg = dyncast<mir::Register*>(arg)) {
            bool argDead = !BB.isLiveIn(argReg);
            for (auto* use: argReg->uses()) {
                argDead &= use == &phi || use->parent() == pred;
            }
            if (argDead) {
                pred->removeLiveOut(argReg);
            }
        }
        pred->addLiveOut(dest);
    }
    return BB.erase(itr);
}

static mir::BasicBlock::Iterator destroySelect(mir::Function& F,
                                               mir::BasicBlock& BB,
                                               mir::BasicBlock::Iterator itr) {
    auto& select   = *itr;
    auto* dest     = select.dest();
    auto* thenVal  = select.operandAt(0);
    auto* elseVal  = select.operandAt(1);
    auto condition = select.instDataAs<mir::CompareOperation>();
    auto* copy     = new mir::Instruction(mir::InstCode::Copy,
                                      dest,
                                          { thenVal },
                                      0,
                                      select.bytewidth());
    auto* cndCopy =
        new mir::Instruction(mir::InstCode::CondCopy,
                             dest,
                             { elseVal },
                             static_cast<uint64_t>(inverse(condition)),
                             select.bytewidth());
    BB.insert(itr, copy);
    BB.insert(itr, cndCopy);
    return BB.erase(itr);
}

void cg::destroySSA(mir::Function& F) {
    mapSSAToVirtualRegisters(F);
    for (auto& BB: F) {
        for (auto itr = BB.begin(); itr != BB.end();) {
            switch (itr->instcode()) {
            case mir::InstCode::Call:
                if (isTailCall(*itr)) {
                    itr = destroySSATailCall(F, BB, itr);
                }
                else {
                    itr = destroySSACall(F, BB, itr);
                }
                break;
            case mir::InstCode::CallExt: {
                itr = destroySSACall(F, BB, itr);
                break;
            }
            case mir::InstCode::Return: {
                itr = destroyReturn(F, BB, itr);
                break;
            }
            case mir::InstCode::Phi: {
                itr = destroyPhi(F, BB, itr);
                break;
            }
            case mir::InstCode::Select: {
                itr = destroySelect(F, BB, itr);
                break;
            }
            default:
                ++itr;
                break;
            }
        }
    }
}
