#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include <range/v3/view.hpp>
#include <scatha/Assembly/Assembler.h>
#include <scatha/Assembly/AssemblyStream.h>
#include <scatha/CodeGen/CodeGen.h>
#include <scatha/Common/ExecutableWriter.h>
#include <scatha/Common/SourceFile.h>
#include <scatha/IR/Context.h>
#include <scatha/IR/Module.h>
#include <scatha/IRGen/IRGen.h>
#include <scatha/Issue/IssueHandler.h>
#include <scatha/Opt/Optimizer.h>
#include <scatha/Parser/Parser.h>
#include <scatha/Sema/Analyze.h>
#include <scatha/Sema/SymbolTable.h>
#include <termfmt/termfmt.h>
#include <utl/format_time.hpp>
#include <utl/strcat.hpp>
#include <utl/streammanip.hpp>
#include <utl/typeinfo.hpp>

#include "CLIParse.h"

using namespace scatha;

/// Writes red "Error: " message to \p str
[[maybe_unused]] static constexpr utl::streammanip Warning(
    [](std::ostream& str) {
    str << tfmt::format(tfmt::Yellow | tfmt::Bold, "Warning: ");
});

/// Writes yellow "Warning: " message to \p str
[[maybe_unused]] static constexpr utl::streammanip Error([](std::ostream& str) {
    str << tfmt::format(tfmt::Red | tfmt::Bold, "Error: ");
});

int main(int argc, char* argv[]) {
    scathac::Options options = scathac::parseCLI(argc, argv);
    if (options.files.empty()) {
        std::cout << Error << "No input files" << std::endl;
        return -1;
    }
    auto sourceFiles = options.files |
                       ranges::views::transform([](auto const& path) {
                           return SourceFile::load(path);
                       }) |
                       ranges::to<std::vector>;

    /// Now we compile the program
    auto const compileBeginTime = std::chrono::high_resolution_clock::now();
    IssueHandler issueHandler;
    auto ast = parser::parse(sourceFiles, issueHandler);
    if (!issueHandler.empty()) {
        issueHandler.print(sourceFiles);
    }
    if (!ast) {
        return -1;
    }
    sema::SymbolTable semaSym;
    auto analysisResult = sema::analyze(*ast, semaSym, issueHandler);
    if (!issueHandler.empty()) {
        issueHandler.print(sourceFiles);
    }
    if (issueHandler.haveErrors()) {
        return -1;
    }
    auto [context, mod] =
        irgen::generateIR(*ast,
                          semaSym,
                          analysisResult,
                          { .sourceFiles = sourceFiles,
                            .generateDebugSymbols = options.debug });
    if (options.optimize) {
        opt::optimize(context, mod, 1);
    }
    auto asmStream = cg::codegen(mod);
    auto [program, symbolTable] = Asm::assemble(asmStream);
    std::string dsym = [&] {
        if (!options.debug) {
            return std::string{};
        }
        return Asm::generateDebugSymbols(asmStream);
    }();
    auto const compileEndTime = std::chrono::high_resolution_clock::now();
    if (options.time) {
        std::cout << "Compilation took "
                  << utl::format_duration(compileEndTime - compileBeginTime)
                  << "\n";
    }

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
    return 0;
}
