#include "SampleCompiler.h"

#include <fstream>
#include <iostream>
#include <sstream>

#include <svm/Program.h>
#include <svm/VirtualMachine.h>
#include <utl/typeinfo.hpp>

#include "AST/LowerToIR.h"
#include "AST/Print.h"
#include "Assembly/Assembler.h"
#include "Assembly/AssemblyStream.h"
#include "Assembly/Print.h"
#include "CodeGen/CodeGen.h"
#include "IR/CFG.h"
#include "IR/Context.h"
#include "IR/Module.h"
#include "IR/Print.h"
#include "IR/Validate.h"
#include "Opt/Inliner.h"
#include "Parser/Lexer.h"
#include "Parser/LexicalIssue.h"
#include "Parser/Parser.h"
#include "Parser/SyntaxIssue.h"
#include "Sema/Analyze.h"
#include "Sema/Print.h"
#include "Sema/SemanticIssue.h"

using namespace scatha;
using namespace scatha::parse;

static void line(std::string_view m) {
    std::cout << "==============================" << m
              << "==============================\n";
};

static void header(std::string_view title = "") {
    std::cout << "\n";
    line("");
    line(title);
    line("");
    std::cout << "\n";
}

static void subHeader(std::string_view title = "") {
    std::cout << "\n";
    line(title);
    std::cout << "\n";
}

void playground::compile(std::filesystem::path filepath) {
    std::fstream file(filepath);
    if (!file) {
        std::cerr << "Failed to open file " << filepath << std::endl;
        return;
    }
    std::stringstream sstr;
    sstr << file.rdbuf();
    compile(sstr.str());
}

void playground::compile(std::string text) {
    // Lexical analysis
    IssueHandler issues;
    auto tokens = parse::lex(text, issues);
    issues.print(text);

    // Parsing
    auto ast = parse::parse(tokens, issues);
    issues.print(text);
    if (!issues.empty()) {
        return;
    }
    printTree(*ast);

    // Semantic analysis
    header(" Symbol Table ");
    sema::SymbolTable sym;
    sema::analyze(*ast, sym, issues);
    issues.print(text);
    if (!issues.empty()) {
        return;
    }
    sema::printSymbolTable(sym);

    subHeader();
    header(" Generated IR ");
    ir::Context irCtx;
    ir::Module mod = ast::lowerToIR(*ast, sym, irCtx);
    ir::print(mod);

    header(" Optimized IR ");
    opt::inlineFunctions(irCtx, mod);
    ir::print(mod);

    header(" Assembly generated from IR ");
    auto const assembly = cg::codegen(mod);
    print(assembly);

    header(" Assembled Program ");

    auto const [program, symbolTable] = Asm::assemble(assembly);
    svm::print(program.data());
    subHeader();

    svm::VirtualMachine vm;
    vm.loadBinary(program.data());
    auto mainPos =
        std::find_if(symbolTable.begin(), symbolTable.end(), [](auto& p) {
            return p.first.starts_with("main");
        });
    if (mainPos == symbolTable.end()) {
        std::cout << "No main function defined!\n";
        return;
    }
    vm.execute(mainPos->second, {});
    u64 const exitCode = vm.getRegister(0);
    std::cout << "VM: Program ended with exit code: [\n\ti: "
              << static_cast<i64>(exitCode) << ", \n\tu: " << exitCode
              << ", \n\tf: " << utl::bit_cast<f64>(exitCode) << "\n]"
              << std::endl;

    subHeader();
}
