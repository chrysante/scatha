#include "Compiler.h"

#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

#include <range/v3/view.hpp>
#include <scatha/Assembly/Assembler.h>
#include <scatha/Assembly/AssemblyStream.h>
#include <scatha/CodeGen/CodeGen.h>
#include <scatha/Common/ExecutableWriter.h>
#include <scatha/Common/SourceFile.h>
#include <scatha/IR/Context.h>
#include <scatha/IR/Module.h>
#include <scatha/IR/Print.h>
#include <scatha/IRGen/IRGen.h>
#include <scatha/Sema/Entity.h>
#include <scatha/Sema/Serialize.h>
#include <utl/format_time.hpp>
#include <utl/strcat.hpp>

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

static std::filesystem::path appendExt(std::filesystem::path p,
                                       std::string_view ext) {
    p += ".";
    p += ext;
    return p;
}

static std::fstream createFile(std::filesystem::path const& path,
                               std::ios::openmode flags = {}) {
    std::filesystem::create_directories(path.parent_path());
    flags |= std::ios::out | std::ios::trunc;
    std::fstream file(path, flags);
    if (!file) {
        throw std::runtime_error(utl::strcat("Failed to create ", path));
    }
    return file;
}

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
    if (options.targetType == TargetType::StaticLibrary) {
        semaSym.globalScope().setName(options.bindir.stem());
    }
    printTime("Frontend");
    irgen::Config irgenConfig = { .sourceFiles = sourceFiles,
                                  .generateDebugSymbols = options.debug };
    if (options.targetType == TargetType::StaticLibrary) {
        irgenConfig.nameMangler = sema::NameMangler(
            { .globalPrefix = options.bindir.stem().string() });
    }
    auto [context, mod] =
        genIR(*ast, semaSym, analysisResult, std::move(irgenConfig));
    printTime("IR generation");
    if (options.optimize) {
        options.optLevel = 1;
    }
    optimize(context, mod, options);
    printTime("Optimizer");

    switch (options.targetType) {
    case TargetType::Executable: {
        auto asmStream = cg::codegen(mod);
        printTime("Codegen");
        auto [program, symbolTable, unresolved] = Asm::assemble(asmStream);
        printTime("Assembler");
        auto linkRes =
            Asm::link(program, semaSym.foreignLibraries(), unresolved);
        if (!linkRes) {
            printLinkerError(linkRes.error());
            return 1;
        }
        printTime("Linker");
        std::string dsym = [&] {
            if (!options.debug) {
                return std::string{};
            }
            return Asm::generateDebugSymbols(asmStream);
        }();
        /// We emit the executable
        writeExecutableFile(options.bindir,
                            program,
                            { .executable = !options.binaryOnly });
        if (options.debug) {
            auto file = createFile(appendExt(options.bindir, "scdsym"));
            file << dsym;
        }
        break;
    }
    case TargetType::StaticLibrary: {
        auto symfile = createFile(appendExt(options.bindir, "scsym"));
        sema::serialize(semaSym, symfile);
        auto irfile = createFile(appendExt(options.bindir, "scir"));
        ir::print(mod, irfile);
        break;
    }
    }

    printTime("Total");
    return 0;
}
