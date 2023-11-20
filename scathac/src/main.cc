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

/// Prints an error message to the console and exits the program
[[noreturn]] static void fileEmissionError(std::string_view type,
                                           std::filesystem::path path) {
    std::cerr << Error << "Failed to emit " << type << "\n";
    std::cerr << "Target was: " << path << "\n";
    std::exit(EXIT_FAILURE);
}

/// Helper to write escaped bash command to a file. See documentation of
/// `writeBashHeader()`
static auto bashCommandEmitter(std::ostream& file) {
    return [&, i = 0](std::string_view command) mutable {
        if (i++ == 0) {
            file << "#!/bin/sh\n";
        }
        else {
            file << "#Shell command\n";
        }
        file << command << "\n";
    };
}

/// To emit files that are directly executable, we prepend a bash script to the
/// emitted binary file. That bash script executes the virtual machine with the
/// same file and exits. The convention for bash commands is one commented line
/// (starting with `#` and ending with `\n`) and one line of script (ending with
/// `\n`). This way the virtual machine identifies the bash commands and ignores
/// them
static void writeBashHeader(std::ostream& file) {
    auto emitter = bashCommandEmitter(file);
    emitter("svm --binary \"$0\" \"$@\"");
    emitter("exit $?");
}

/// Copies the program \p program to the file \p file
static void writeBinary(std::ostream& file, std::span<uint8_t const> program) {
    std::copy(program.begin(),
              program.end(),
              std::ostream_iterator<char>(file));
}

/// Calls the system command `chmod` to permit execution of the specified file
static void permitExecution(std::filesystem::path filename) {
    std::system(utl::strcat("chmod +x ", filename.string()).data());
}

/// Revokes permission to execute the specified file
static void prohibitExecution(std::filesystem::path filename) {
    std::system(utl::strcat("chmod -x ", filename.string()).data());
}

/// Emits the compiled binary.
/// \param dest The file output filename
/// \param program The compiled binary
/// \param executable If true, creates a directly executable file of our binary
static void emitFile(std::filesystem::path dest,
                     std::span<uint8_t const> program,
                     bool executable) {
    if (executable) {
        std::fstream file(dest, std::ios::out | std::ios::trunc);
        if (!file) {
            fileEmissionError("executable", dest);
        }
        writeBashHeader(file);
        file.close();
        permitExecution(dest);
    }
    /// We open the file again, this time in binary mode, to ensure that no
    /// unwanted character conversions occur
    auto const flags = std::ios::out | std::ios::binary |
                       (executable ? std::ios::app : std::ios::trunc);
    std::fstream file(dest, flags);
    if (!file) {
        fileEmissionError("binary", dest);
    }
    file.seekg(0, std::ios::end);
    writeBinary(file, program);
    file.close();
    /// If we don't generate an executable, we explicitly revoke permission to
    /// execute the file, because the same file could have been made executable
    /// by a previous invocation of the compiler
    if (!executable) {
        prohibitExecution(dest);
    }
}

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
    emitFile(options.bindir, program, /* executable = */ !options.binaryOnly);
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
