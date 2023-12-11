#include "CodeGen/CodeGen.h"

#include <memory>

#include <range/v3/algorithm.hpp>

#include "Assembly/AssemblyStream.h"
#include "CodeGen/Passes.h"
#include "MIR/CFG.h"
#include "MIR/Context.h"
#include "MIR/Module.h"

using namespace scatha;
using namespace cg;

Asm::AssemblyStream cg::codegen(ir::Module const& irMod) {
    return codegen(irMod, *std::make_unique<NullLogger>());
}

static void forEach(mir::Context& ctx, mir::Module& mod, auto transform) {
    ranges::for_each(mod, [&](auto& F) { transform(ctx, F); });
}

Asm::AssemblyStream cg::codegen(ir::Module const& irMod, cg::Logger& logger) {
    mir::Context ctx;
    auto mod = cg::lowerToMIR2(ctx, irMod);
    logger.log("Initial MIR module", mod);

    forEach(ctx, mod, cg::instSimplify);
    logger.log("MIR module after simplification", mod);

    forEach(ctx, mod, cg::commonSubexpressionElimination);
    logger.log("MIR module after CSE", mod);

    forEach(ctx, mod, cg::deadCodeElim);
    logger.log("MIR module after DCE", mod);

    /// We compute live sets just before we leave SSA form
    forEach(ctx, mod, cg::computeLiveSets);
    forEach(ctx, mod, cg::destroySSA);
    logger.log("MIR module after SSA destructions", mod);

    forEach(ctx, mod, cg::allocateRegisters);
    logger.log("MIR module after register allocation", mod);

    forEach(ctx, mod, cg::elideJumps);
    logger.log("MIR module after jump elision", mod);
    return cg::lowerToASM(mod);
}
