#include "MIR/Devirtualize.h"

#include "MIR/CFG.h"

using namespace scatha;
using namespace mir;

/// Instruction pointer, register pointer offset and stack pointer
static constexpr size_t NumRegsForCallMetadata = 3;

bool mir::devirtualize(mir::Function& F) {
    Register* last = F.registers().back();
    size_t idx     = last->index() + NumRegsForCallMetadata;
    for (auto virtReg = F.virtRegBegin(), end = F.virtRegEnd(); virtReg != end;
         ++virtReg, ++idx)
    {
        auto* r = new Register(idx);
        F.addRegister(r);
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
