#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include <utl/typeinfo.hpp>
#include <utl/stdio.hpp>

#include "AST/PrintSource.h"
#include "AST/PrintTree.h"
#include "AST/PrintExpression.h"
#include "Assembly/Assembler.h"
#include "Assembly/AssemblyUtil.h"
#include "CodeGen/CodeGenerator.h"
#include "IC/Canonicalize.h"
#include "IC/PrintTac.h"
#include "IC/TacGenerator.h"
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

void line(std::string_view m) { utl::print("{:=^{}}\n", m, headerWidth); };

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

[[gnu::weak]]
int main() {
    auto const filepath = std::filesystem::path(PROJECT_LOCATION) / "playground/Test.sc";
    std::fstream file(filepath);
    if (!file) {
        std::cout << "Failed to open file " << filepath << std::endl;
    }

    std::stringstream sstr;
    sstr << file.rdbuf();
    std::string const text = sstr.str();

    try {
        header(" Tokens ");
        issue::LexicalIssueHandler lexIss;
        auto tokens = lex::lex(text, lexIss);
        if (!lexIss.empty()) {
            utl::print("Lexical issues:\n");
            for (auto& issue: lexIss.issues()) {
                issue.visit([]<typename T>(T const& iss) {
                    std::cout << iss.token().sourceLocation << " " << iss.token() << " : "
                              << utl::nameof<T> << std::endl;
                });
            }
        }
        else {
            utl::print("No lexical issues.\n");
        }
        
        header(" AST ");
        issue::SyntaxIssueHandler parseIss;
        auto ast = parse::parse(tokens, parseIss);
        ast::printTree(*ast);

        header(" Symbol Table ");
        issue::SemaIssueHandler semaIss;
        auto const sym = sema::analyze(*ast, semaIss);
        sema::printSymbolTable(sym);
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
        subHeader();
        header(" Generated Three Address Code ");
        ic::canonicalize(ast.get());
        auto const tac = ic::generateTac(*ast, sym);
        ic::printTac(tac, sym);

        header(" Generated Assembly ");
        codegen::CodeGenerator cg(tac);
        auto const str = cg.run();
        print(str, sym);

        
        header(" Assembled Program ");
        assembly::Assembler a(str);
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
            return -1;
        }
        auto const program = a.assemble({ .mainID = mainID.rawValue() });
        print(program);

        subHeader();

        vm::VirtualMachine vm;
        vm.load(program);
        vm.execute();
        u64 const exitCode = vm.getState().registers[0];
        std::cout << "VM: Program ended with exit code: " << static_cast<i64>(exitCode) << std::endl;

        subHeader();
    }
    catch (std::exception const& e) {
        std::cout << "Compilation failed:\n" << e.what() << std::endl;
    }
}
