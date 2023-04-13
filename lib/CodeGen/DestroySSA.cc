#include "CodeGen/DestroySSA.h"

#include <utl/hashtable.hpp>

#include "MIR/CFG.h"
#include "MIR/Register.h"

using namespace scatha;
using namespace cg;

void cg::destroySSA(mir::Function& F) {
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
    /// Remove all `phi` and `select` instructions and insert respective copies.
    for (auto& BB: F) {
        for (auto itr = BB.begin(); itr != BB.end();) {
            if (itr->instcode() == mir::InstCode::Phi) {
                auto& phi  = *itr;
                auto* dest = phi.dest();
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
                    /// Update live sets to honor inserted copy.
                    pred->insert(before,
                                 new mir::Instruction(mir::InstCode::Copy,
                                                      dest,
                                                      { arg },
                                                      0,
                                                      phi.bytewidth()));
                    auto* argReg = dyncast<mir::Register*>(arg);
                    if (argReg && !BB.isLiveIn(argReg)) {
                        pred->removeLiveOut(argReg);
                    }
                    pred->addLiveOut(dest);
                }
                itr = BB.erase(itr);
                continue;
            }
            if (itr->instcode() == mir::InstCode::Select) {
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
                auto* cndCopy  = new mir::Instruction(mir::InstCode::CondCopy,
                                                     dest,
                                                      { elseVal },
                                                     static_cast<uint64_t>(
                                                         inverse(condition)),
                                                     select.bytewidth());
                BB.insert(itr, copy);
                BB.insert(itr, cndCopy);
                itr = BB.erase(itr);
                continue;
            }
            ++itr;
        }
    }
}
