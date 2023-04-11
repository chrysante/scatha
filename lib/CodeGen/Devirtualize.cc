#include "CodeGen/Devirtualize.h"

#include "MIR/CFG.h"

using namespace scatha;
using namespace mir;

/// Instruction pointer, register pointer offset and stack pointer
static constexpr size_t NumRegsForCallMetadata = 3;

// TODO: Rename to devirtualizeCalls()
bool cg::devirtualize(mir::Function& F) {
    F.setNumLocalRegisters(F.numUsedRegisters());
    for (size_t i = 0; i < NumRegsForCallMetadata; ++i) {
        F.addRegister();
    }
    for (auto virtReg = F.virtRegBegin(), end = F.virtRegEnd(); virtReg != end;
         ++virtReg)
    {
        auto* r = F.addRegister();
        while (!virtReg->defs().empty()) {
            auto* def = virtReg->defs().front();
            def->setDest(r);
        }
        while (!virtReg->uses().empty()) {
            auto* user = virtReg->uses().front();
            user->replaceOperand(virtReg.to_address(), r);
        }
    }
    bool const changedAny = F.virtRegBegin() != F.virtRegEnd();
    F.clearVirtualRegisters();
    return changedAny;
}
