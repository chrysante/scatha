#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include <scatha/AST/LowerToIR.h>
#include <scatha/AST/Print.h>
#include <scatha/Assembly/Assembler.h>
#include <scatha/Assembly/AssemblyStream.h>
#include <scatha/CodeGen/CodeGen.h>
#include <scatha/IR/Context.h>
#include <scatha/IR/Module.h>
#include <scatha/Issue/Format.h>
#include <scatha/Issue/IssueHandler.h>
#include <scatha/Opt/Optimizer.h>
#include <scatha/Parser/Parser.h>
#include <scatha/Sema/Analyze.h>
#include <termfmt/termfmt.h>
#include <utl/format_time.hpp>
#include <utl/typeinfo.hpp>

#include "CLIParse.h"

using namespace scatha;

int main(int argc, char* argv[]) {
    scathac::Options options = scathac::parseCLI(argc, argv);
    std::fstream file(options.filepath, std::ios::in);
    if (!file) {
        std::cout << "Failed to open " << options.filepath << "\n";
        return -1;
    }
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
    sema::analyze(*ast, semaSym, issueHandler);
    if (!issueHandler.empty()) {
        issueHandler.print(text);
        return -1;
    }

    /// Generate IR
    ir::Context context;
    auto mod = ast::lowerToIR(*ast, semaSym, context);

    opt::optimize(context, mod, options.optLevel);

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

    if (options.objpath.empty()) {
        options.objpath = options.filepath.parent_path() /
                          options.filepath.stem().concat(".sbin");
    }
    std::fstream out(options.objpath,
                     std::ios::out | std::ios::trunc | std::ios::binary);
    if (!out) {
        std::cout << "Failed to emit binary\n";
        std::cout << "Target was: " << options.objpath << "\n";
        return -1;
    }
    std::copy(program.begin(), program.end(), std::ostream_iterator<char>(out));
}
