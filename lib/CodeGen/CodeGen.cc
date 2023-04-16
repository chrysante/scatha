#include "CodeGen/CodeGen.h"

#include "Assembly/AssemblyStream.h"
#include "CodeGen/DataFlow.h"
#include "CodeGen/DeadCodeElim.h"
#include "CodeGen/DestroySSA.h"
#include "CodeGen/JumpElision.h"
#include "CodeGen/LowerToASM.h"
#include "CodeGen/LowerToMIR.h"
#include "CodeGen/RegisterAllocator.h"
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
