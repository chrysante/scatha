#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include <scatha/Assembly/Assembler.h>
#include <scatha/Assembly/AssemblyStream.h>
#include <scatha/CodeGen/CodeGen.h>
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
static constexpr utl::streammanip Warning([](std::ostream& str) {
    str << tfmt::format(tfmt::Yellow | tfmt::Bold, "Warning: ");
});

/// Writes yellow "Warning: " message to \p str
static constexpr utl::streammanip Error([](std::ostream& str) {
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
            file << "#Bash command\n";
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
    emitter("svm \"$0\"");
    emitter("exit $1");
}

/// Copies the program \p program to the file \p file
static void writeBinary(std::ostream& file, std::span<uint8_t const> program) {
    std::copy(program.begin(),
              program.end(),
              std::ostream_iterator<char>(file));
}

/// Calls the system command `chmod` to permit execution of the emitted file
static void signExecutable(std::filesystem::path filename) {
    std::system(utl::strcat("chmod +x ", filename.string()).data());
}

/// Creates a directly executable file of our binary
static void emitExecutable(std::filesystem::path dest,
                           std::span<uint8_t const> program) {
    std::fstream file(dest, std::ios::out | std::ios::trunc);
    if (!file) {
        fileEmissionError("executable", dest);
    }
    writeBashHeader(file);
    file.close();
    file.open(dest, std::ios::out | std::ios::binary | std::ios::app);
    if (!file) {
        fileEmissionError("binary", dest);
    }
    file.seekg(0, std::ios::end);
    writeBinary(file, program);
    file.close();
    signExecutable(dest);
}

[[maybe_unused]] static void emitBinary(std::filesystem::path dest,
                                        std::span<uint8_t const> program) {
    std::fstream file(dest, std::ios::out | std::ios::trunc | std::ios::binary);
    if (!file) {
        fileEmissionError("binary", dest);
    }
    std::copy(program.begin(),
              program.end(),
              std::ostream_iterator<char>(file));
}

int main(int argc, char* argv[]) {
    scathac::Options options = scathac::parseCLI(argc, argv);
    if (options.files.empty()) {
        std::cout << Error << "No input files" << std::endl;
        return -1;
    }
    if (options.files.size() > 1) {
        std::cout << Warning
                  << "All input files but the first are ignored for now"
                  << std::endl;
    }
    auto const filepath = options.files.front();
    std::fstream file(filepath, std::ios::in);
    assert(file && "CLI11 library should have caught this");

    std::stringstream sstr;
    sstr << file.rdbuf();
    std::string const text = std::move(sstr).str();

    auto const compileBeginTime = std::chrono::high_resolution_clock::now();

    IssueHandler issueHandler;
    auto ast = parse::parse(text, issueHandler);
    if (!issueHandler.empty()) {
        issueHandler.print(text);
    }
    if (!ast) {
        return -1;
    }

    /// Analyse the AST
    sema::SymbolTable semaSym;
    auto analysisResult = sema::analyze(*ast, semaSym, issueHandler);
    if (!issueHandler.empty()) {
        issueHandler.print(text);
    }
    if (issueHandler.haveErrors()) {
        return -1;
    }

    /// Generate IR
    auto [context, mod] = irgen::generateIR(*ast, semaSym, analysisResult);

    if (options.optimize) {
        opt::optimize(context, mod, 1);
    }

    /// Generate assembly
    auto asmStream = cg::codegen(mod);

    /// Assemble program
    auto [program, symbolTable] = Asm::assemble(asmStream);

    if (options.time) {
        auto const compileEndTime = std::chrono::high_resolution_clock::now();
        std::cout << "Compilation took "
                  << utl::format_duration(compileEndTime - compileBeginTime)
                  << "\n";
    }

    /// Emit executable
    if (options.bindir.empty()) {
        options.bindir = filepath.parent_path();
    }
    std::string const execName = filepath.stem().string();
    std::filesystem::path const execDir = options.bindir / execName;
    emitExecutable(execDir, program);
    return 0;
}
