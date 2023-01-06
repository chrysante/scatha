#include "SampleCompiler.h"

#include <fstream>
#include <iostream>
#include <sstream>

#include <utl/typeinfo.hpp>
#include <utl/stdio.hpp>
#include <utl/format.hpp>

#include "AST/PrintSource.h"
#include "AST/PrintTree.h"
#include "AST/PrintExpression.h"
#include "Parser/SyntaxIssue.h"
#include "Assembly/Assembler.h"
#include "Assembly/AssemblyStream.h"
#include "Assembly/Print.h"
#include "ASTCodeGen/CodeGen.h"
#include "IR/Module.h"
#include "IR/CFG.h"
#include "IR/Context.h"
#include "IR/Print.h"
#include "CodeGen/CodeGenerator.h"
#include "Lexer/Lexer.h"
#include "Lexer/LexicalIssue.h"
#include "Parser/Parser.h"
#include "Sema/Analyze.h"
#include "Sema/PrintSymbolTable.h"
#include "VM/VirtualMachine.h"

using namespace scatha;
using namespace scatha::lex;
using namespace scatha::parse;

namespace internal {

static int const headerWidth = 60;

static void line(std::string_view m) { utl::print("{:=^{}}\n", m, headerWidth); };

}

static void header(std::string_view title = "") {
    utl::print("\n");
    ::internal::line("");
    ::internal::line(title);
    ::internal::line("");
    utl::print("\n");
}

static void subHeader(std::string_view title = "") {
    utl::print("\n");
    ::internal::line(title);
    utl::print("\n");
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
    header(" Tokens ");
    issue::LexicalIssueHandler lexIss;
    auto tokens = lex::lex(text, lexIss);
    if (lexIss.empty()) {
        utl::print("No lexical issues.\n");
    }
    else {
        utl::print("Lexical issues:\n");
        for (auto& issue: lexIss.issues()) {
            issue.visit([]<typename T>(T const& iss) {
                std::cout << iss.token().sourceLocation << " " << iss.token() << " : "
                          << utl::nameof<T> << std::endl;
            });
        }
    }

    // Parsing
    header(" AST ");
    issue::SyntaxIssueHandler parseIss;
    auto ast = parse::parse(tokens, parseIss);
    if (parseIss.empty()) {
        utl::print("No syntax issues.\n");
    }
    else {
        utl::print("\nEncoutered {} issues:\n", parseIss.issues().size());
        for (SyntaxIssue const& issue : parseIss.issues()) {
            auto const loc = issue.token().sourceLocation;
            std::cout << "\tLine " << loc.line << " Col " << loc.column << ": ";
            std::cout << issue.reason() << "\n";
        }
    }

    // Semantic analysis
    header(" Symbol Table ");
    issue::SemaIssueHandler semaIss;
    auto const sym = sema::analyze(*ast, semaIss);
    if (semaIss.issues().empty()) {
        utl::print("No semantic issues.\n");
    }
    else {
        std::cout << "\nEncoutered " << semaIss.issues().size() << " issues\n";
        subHeader();
        for (auto const& issue : semaIss.issues()) {
            issue.visit([](auto const& issue) {
                auto const loc = issue.token().sourceLocation;
                std::cout << "Line " << loc.line << " Col " << loc.column << ": ";
                std::cout << utl::nameof<std::decay_t<decltype(issue)>> << "\n\t";
            });
            issue.visit(utl::visitor{ [&](sema::InvalidDeclaration const& e) {
                                         std::cout << "Invalid declaration (" << e.reason() << "): ";
                                         std::cout << std::endl;
                                     },
                                      [&](sema::BadTypeConversion const& e) {
                std::cout << "Bad type conversion: ";
                ast::printExpression(e.expression());
                std::cout << std::endl;
                std::cout << "\tFrom " << sym.getName(e.from()) << " to " << sym.getName(e.to()) << "\n";
                                     },
                                      [&](sema::BadFunctionCall const& e) {
                std::cout << "Bad function call: " << e.reason() << ": ";
                ast::printExpression(e.expression());
                std::cout << std::endl;
            },
                        [&](sema::UseOfUndeclaredIdentifier const& e) {
                std::cout << "Use of undeclared identifier ";
                ast::printExpression(e.expression());
                std::cout << " in scope: " << e.currentScope().name() << std::endl;
            },
                [](issue::ProgramIssueBase const&) { std::cout << std::endl; } });
            std::cout << std::endl;
        }
    }

    if (!lexIss.empty() || !parseIss.empty() || !semaIss.empty()) { return; }

    subHeader();
    header(" Generated IR ");
    ir::Context irCtx;
    ir::Module mod = ast::codegen(*ast, sym, irCtx);
    ir::print(mod);
    
    header(" Assembly generated from IR ");
    auto const str0 = cg::codegen(mod);
    print(str0);
    
    header(" Assembled Program ");
    /// Start execution with main if it exists.
    auto const mainID = [&sym] {
        auto const id  = sym.lookup("main");
        auto const* os = sym.tryGetOverloadSet(id);
        if (!os) {
            return sema::SymbolID::Invalid;
        }
        auto const* mainFn = os->find({});
        if (!mainFn) {
            return sema::SymbolID::Invalid;
        }
        return mainFn->symbolID();
    }();
    if (!mainID) {
        std::cout << "No main function defined!\n";
        return;
    }
    auto const program = Asm::assemble(str0, { .startFunction = utl::format("main{:x}", mainID.rawValue()) });
    print(program);
    subHeader();

    vm::VirtualMachine vm;
    vm.load(program);
    vm.execute();
    u64 const exitCode = vm.getState().registers[0];
    std::cout << "VM: Program ended with exit code: [\n\ti: " << static_cast<i64>(exitCode) <<
                                                  ", \n\tu: " << exitCode <<
                                                  ", \n\tf: " << utl::bit_cast<f64>(exitCode) << "\n]" << std::endl;

    subHeader();
}
