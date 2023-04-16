#include "CodeGen/CodeGen.h"

#include "Assembly/AssemblyStream.h"
#include "CodeGen/Passes.h"
#include "MIR/CFG.h"
#include "MIR/Module.h"

using namespace scatha;
using namespace cg;

Asm::AssemblyStream cg::codegen(ir::Module const& irMod) {
    auto mod = cg::lowerToMIR(irMod);
    for (auto& F: mod) {
        cg::computeLiveSets(F);
        cg::deadCodeElim(F);
        cg::destroySSA(F);
        cg::allocateRegisters(F);
        cg::elideJumps(F);
    }
    return cg::lowerToASM(mod);
}
