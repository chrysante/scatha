#include "CodeGen/Devirtualize.h"

#include "MIR/CFG.h"

using namespace scatha;
using namespace mir;

/// Instruction pointer, register pointer offset and stack pointer
static constexpr size_t NumRegsForCallMetadata = 3;

bool cg::devirtualizeCalls(mir::Function& F) {
    F.setNumLocalRegisters(F.numUsedRegisters());
    for (size_t i = 0; i < NumRegsForCallMetadata; ++i) {
        F.addRegister();
    }
    for (auto calleeReg = F.calleeRegsBegin(), end = F.calleeRegsEnd();
         calleeReg != end;
         ++calleeReg)
    {
        auto* r = F.addRegister();
        while (!calleeReg->defs().empty()) {
            auto* def = calleeReg->defs().front();
            def->setDest(r);
        }
        while (!calleeReg->uses().empty()) {
            auto* user = calleeReg->uses().front();
            user->replaceOperand(calleeReg.to_address(), r);
        }
    }
    bool const changedAny = F.calleeRegsBegin() != F.calleeRegsEnd();
    F.clearCalleeRegisters();
    return changedAny;
}
