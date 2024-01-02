#include "Inspect.h"

#include <fstream>
#include <iostream>

#include <scatha/AST/Print.h>
#include <scatha/Assembly/Assembler.h>
#include <scatha/Assembly/AssemblyStream.h>
#include <scatha/CodeGen/CodeGen.h>
#include <scatha/CodeGen/Logger.h>
#include <scatha/CodeGen/Passes.h>
#include <scatha/Common/ExecutableWriter.h>
#include <scatha/Common/Logging.h>
#include <scatha/IR/Context.h>
#include <scatha/IR/Module.h>
#include <scatha/IR/Print.h>
#include <scatha/IRGen/IRGen.h>
#include <scatha/MIR/Context.h>
#include <scatha/MIR/Module.h>
#include <scatha/MIR/Print.h>
#include <scatha/Sema/Print.h>

#include "Util.h"

using namespace scatha;
using namespace logging;

static void iselViz(ir::Module const& irMod) {
    mir::Context ctx;
    auto mirMod =
        cg::lowerToMIR(ctx, irMod, { .generateSelectionDAGImages = true });
    header("Generated MIR");
    mir::print(mirMod);
    std::cout
        << "Warning: Other codegen options are ignored with the --isel flag"
        << std::endl;
}

int scatha::inspectMain(InspectOptions options) {
    CompilerInvocation invocation;
    invocation.setInputs(loadSourceFiles(options.files));
    invocation.setLibSearchPaths(options.libSearchPaths);
    // clang-format off
    invocation.setCallbacks({
        .frontendCallback = [&](ast::ASTNode const& ast, 
                                sema::SymbolTable const& sym) {
            if (options.ast) {
                header("AST");
                ast::printTree(ast);
            }
            if (options.sym) {
                header("Symbol Table");
                sema::print(sym);
            }
        },
        .optCallback = [&](ir::Context const&, ir::Module const& mod) {
            if (options.emitIR) {
                if (auto file = std::fstream("out.scir",
                                             std::ios::out | std::ios::trunc))
                {
                    ir::print(mod, file);
                }
                else {
                    std::cout << "Failed to write \"out.scir\"" << std::endl;
                }
            }
            if (options.isel) {
                iselViz(mod);
                invocation.stop();
            }
        },
        .codegenCallback = [&](Asm::AssemblyStream const& program) {
            if (options.assembly) {
                header("Assembly");
                Asm::print(program);
            }
        },
    }); // clang-format on
    invocation.setTargetType(options.targetType);
    invocation.setFrontend(deduceFrontend(options.files));
    invocation.setOutputFile(options.outputFile);
    invocation.setOptLevel(options.optLevel);
    invocation.setOptPipeline(options.pipeline);
    cg::DebugLogger codegenLogger;
    if (options.codegen) {
        invocation.setCodegenLogger(codegenLogger);
    }
    return invocation.run();
}
