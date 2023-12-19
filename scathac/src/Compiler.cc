#include "Compiler.h"

#include <chrono>
#include <fstream>
#include <iostream>
#include <vector>

#include <range/v3/view.hpp>
#include <scatha/Assembly/Assembler.h>
#include <scatha/Assembly/AssemblyStream.h>
#include <scatha/CodeGen/CodeGen.h>
#include <scatha/Common/ExecutableWriter.h>
#include <scatha/Common/SourceFile.h>
#include <scatha/IR/Context.h>
#include <scatha/IR/Module.h>
#include <scatha/IRGen/IRGen.h>
#include <utl/format_time.hpp>

#include "Util.h"

using namespace scatha;
using namespace ranges::views;

namespace {

struct Timer {
    using Clock = std::chrono::high_resolution_clock;

    Timer() { reset(); }

    Clock::duration elapsed() const { return Clock::now() - start; }

    void reset() { start = Clock::now(); }

    Clock::time_point start;
};

} // namespace

int scatha::compilerMain(CompilerOptions options) {
    if (options.files.empty()) {
        std::cout << Error << "No input files" << std::endl;
        return -1;
    }
    auto sourceFiles =
        options.files |
        transform([](auto const& path) { return SourceFile::load(path); }) |
        ranges::to<std::vector>;
    /// Now we compile the program
    Timer timer;
    auto printTime = [&](std::string_view section) {
        if (options.time) {
            std::cout << section << ": "
                      << utl::format_duration(timer.elapsed()) << std::endl;
            timer.reset();
        }
    };
    auto data = parseScatha(sourceFiles, options.libSearchPaths);
    if (!data) {
        return 1;
    }
    auto& [ast, semaSym, analysisResult] = *data;
    printTime("Frontend");
    auto [context, mod] = genIR(*ast,
                                semaSym,
                                analysisResult,
                                { .sourceFiles = sourceFiles,
                                  .generateDebugSymbols = options.debug });
    printTime("IR generation");
    if (options.optimize) {
        options.optLevel = 1;
    }
    optimize(context, mod, options);
    printTime("Optimizer");
    auto asmStream = cg::codegen(mod);
    printTime("Codegen");
    auto [program, symbolTable] = Asm::assemble(asmStream);
    printTime("Assembler");
    std::string dsym = [&] {
        if (!options.debug) {
            return std::string{};
        }
        return Asm::generateDebugSymbols(asmStream);
    }();
    /// We emit the executable
    if (options.bindir.empty()) {
        options.bindir = "out";
    }
    writeExecutableFile(options.bindir,
                        program,
                        { .executable = !options.binaryOnly });
    if (options.debug) {
        auto path = options.bindir;
        path += ".scdsym";
        std::fstream file(path, std::ios::out | std::ios::trunc);
        if (!file) {
            std::cerr << "Failed to write debug symbols\n";
            return 1;
        }
        file << dsym;
    }
    printTime("Total");
    return 0;
}
