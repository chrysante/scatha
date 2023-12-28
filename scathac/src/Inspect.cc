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

int scatha::inspectMain(InspectOptions options) {
    using namespace logging;
    ir::Context ctx;
    ir::Module mod;
    std::vector<std::filesystem::path> foreignLibs;
    switch (getMode(options)) {
    case ParseMode::Scatha: {
        auto data = parseScatha(options);
        if (!data) {
            return 1;
        }
        if (options.ast) {
            header("AST");
            ast::printTree(*data->ast);
        }
        if (options.sym) {
            header("Symbol Table");
            sema::print(data->sym);
        }
        std::tie(ctx, mod) = genIR(*data->ast,
                                   data->sym,
                                   data->analysisResult,
                                   { .generateDebugSymbols = false });
        foreignLibs = data->sym.foreignLibraries() | ranges::to<std::vector>;
        break;
    }
    case ParseMode::IR:
        std::tie(ctx, mod) = parseIR(options);
        break;
    }
    optimize(ctx, mod, options);
    if (options.emitIR) {
        if (auto file =
                std::fstream("out.scir", std::ios::out | std::ios::trunc)) {
            ir::print(mod, file);
        }
        else {
            std::cout << "Failed to write \"out.scir\"" << std::endl;
        }
    }
    if (options.isel) {
        mir::Context ctx;
        auto mirMod =
            cg::lowerToMIR(ctx, mod, { .generateSelectionDAGImages = true });
        std::cout << "Warning: Other codegen options and execution are ignored "
                     "with the --isel flag"
                  << std::endl;
        header("Generated MIR");
        mir::print(mirMod);
        return 0;
    }
    auto cgLogger = [&]() -> std::unique_ptr<cg::Logger> {
        if (options.codegen) {
            return std::make_unique<cg::DebugLogger>();
        }
        return std::make_unique<cg::NullLogger>();
    }();
    auto asmStream = cg::codegen(mod, *cgLogger);
    if (options.assembly) {
        header("Assembly");
        Asm::print(asmStream);
    }
    if (options.out) {
        auto [program, symbolTable, unresolved] = Asm::assemble(asmStream);
        auto linkRes = Asm::link(program, foreignLibs, unresolved);
        if (!linkRes) {
            printLinkerError(linkRes.error());
        }
        writeExecutableFile(*options.out, program);
    }
    return 0;
}
