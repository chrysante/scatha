#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include <scatha/AST/Print.h>
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
#include <utl/streammanip.hpp>
#include <utl/typeinfo.hpp>

#include "CLIParse.h"

using namespace scatha;

static constexpr utl::streammanip Warning([](std::ostream& str) {
    str << tfmt::format(tfmt::Yellow | tfmt::Bold, "Warning: ");
});

static constexpr utl::streammanip Error([](std::ostream& str) {
    str << tfmt::format(tfmt::Red | tfmt::Bold, "Error: ");
});

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

    if (options.bindir.empty()) {
        options.bindir = filepath.parent_path();
    }
    std::filesystem::path binary =
        options.bindir / filepath.stem().concat(".sbin");
    std::fstream out(binary,
                     std::ios::out | std::ios::trunc | std::ios::binary);
    if (!out) {
        std::cout << "Failed to emit binary\n";
        std::cout << "Target was: " << binary << "\n";
        return -1;
    }
    std::copy(program.begin(), program.end(), std::ostream_iterator<char>(out));
    return 0;
}
