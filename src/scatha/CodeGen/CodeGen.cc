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

Asm::AssemblyStream cg::codegen(ir::Module const& irMod,
                                CodegenOptions options) {
    return codegen(irMod, options, *std::make_unique<NullLogger>());
}

static void forEach(mir::Context& ctx, mir::Module& mod,
                    CodegenOptions const& options, auto transform) {
    ranges::for_each(mod, [&](auto& F) { transform(ctx, F, options); });
}

Asm::AssemblyStream cg::codegen(ir::Module const& irMod, CodegenOptions options,
                                cg::Logger& logger) {
    mir::Context ctx;
    auto mod = cg::lowerToMIR(ctx, irMod);
    logger.log("Initial MIR module", mod);

    if (options.optLevel > 0) {
        forEach(ctx, mod, options, cg::instSimplify);
        logger.log("MIR module after simplification", mod);
        forEach(ctx, mod, options, cg::commonSubexpressionElimination);
        logger.log("MIR module after CSE", mod);
        forEach(ctx, mod, options, cg::deadCodeElim);
        logger.log("MIR module after DCE", mod);
    }

    /// We compute live sets just before we leave SSA form
    forEach(ctx, mod, options, cg::computeLiveSets);
    logger.log("MIR module after life set computation", mod);

    forEach(ctx, mod, options, cg::destroySSA);
    logger.log("MIR module after SSA destruction", mod);

    if (options.optLevel > 0) {
        forEach(ctx, mod, options, cg::coalesceCopies);
        logger.log("MIR module after copy coalescing", mod);
    }

    forEach(ctx, mod, options, cg::allocateRegisters);
    logger.log("MIR module after register allocation", mod);

    if (options.optLevel > 0) {
        forEach(ctx, mod, options, cg::elideJumps);
        logger.log("MIR module after jump elision", mod);
    }
    return cg::lowerToASM(mod);
}
