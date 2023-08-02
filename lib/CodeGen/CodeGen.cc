#include "CodeGen/CodeGen.h"

#include <memory>

#include <range/v3/algorithm.hpp>

#include "Assembly/AssemblyStream.h"
#include "CodeGen/Passes.h"
#include "MIR/CFG.h"
#include "MIR/Module.h"

using namespace scatha;
using namespace cg;

Asm::AssemblyStream cg::codegen(ir::Module const& irMod) {
    return codegen(irMod, *std::make_unique<NullLogger>());
}

Asm::AssemblyStream cg::codegen(ir::Module const& irMod, cg::Logger& logger) {
    auto mod = cg::lowerToMIR(irMod);
    logger.log("Initial MIR module", mod);

    ranges::for_each(mod, cg::computeLiveSets);
    ranges::for_each(mod, cg::deadCodeElim);
    logger.log("MIR module after DCE", mod);

    ranges::for_each(mod, cg::destroySSA);
    logger.log("MIR module after SSA destructions", mod);

    ranges::for_each(mod, cg::allocateRegisters);
    logger.log("MIR module after register allocation", mod);

    ranges::for_each(mod, cg::elideJumps);
    logger.log("MIR module after jump elision", mod);
    return cg::lowerToASM(mod);
}
