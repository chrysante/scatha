#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include <scatha/AST/Print.h>
#include <scatha/Assembly/Assembler.h>
#include <scatha/Assembly/AssemblyStream.h>
#include <scatha/CodeGen/AST2IR/CodeGenerator.h>
#include <scatha/CodeGen/IR2ByteCode/CodeGenerator.h>
#include <scatha/IR/Context.h>
#include <scatha/IR/Module.h>
#include <scatha/Lexer/Lexer.h>
#include <scatha/Lexer/LexicalIssue.h>
#include <scatha/Opt/Optimizer.h>
#include <scatha/Parser/Parser.h>
#include <scatha/Parser/SyntaxIssue.h>
#include <scatha/Sema/Analyze.h>
#include <scatha/Sema/SemanticIssue.h>
#include <termfmt/termfmt.h>
#include <utl/format.hpp>
#include <utl/typeinfo.hpp>

#include "CLIParse.h"

using namespace scatha;

static void printIssueEncounters(size_t count, std::string_view kind) {
    tfmt::format(tfmt::brightRed, std::cout, [&] {
        std::cout << "\nEncoutered " << count << " " << kind << " issue"
                  << (count == 1 ? "" : "s") << "\n\n";
    });
}

static void printLexicalIssues(issue::LexicalIssueHandler const& iss) {
    if (iss.empty()) {
        return;
    }
    printIssueEncounters(iss.issues().size(), "lexical");
    for (auto& issue: iss.issues()) {
        issue.visit([]<typename T>(T const& iss) {
            std::cout << iss.token().sourceLocation << " " << iss.token()
                      << " : " << utl::nameof<T> << std::endl;
        });
    }
}

static void printSyntaxIssues(issue::SyntaxIssueHandler const& iss) {
    if (iss.empty()) {
        return;
    }
    printIssueEncounters(iss.issues().size(), "syntactic");
    for (auto const& issue: iss.issues()) {
        auto const loc = issue.token().sourceLocation;
        std::cout << "\tLine " << loc.line << " Col " << loc.column << ": ";
        std::cout << issue.reason() << "\n";
    }
}

static void printSemaIssues(issue::SemaIssueHandler const& iss,
                            sema::SymbolTable const& sym) {
    if (iss.issues().empty()) {
        return;
    }
    printIssueEncounters(iss.issues().size(), "semantic");
    for (auto const& issue: iss.issues()) {
        issue.visit([](auto const& issue) {
            auto const loc = issue.token().sourceLocation;
            std::cout << "Line " << loc.line << " Col " << loc.column << ": ";
            std::cout << utl::nameof<std::decay_t<decltype(issue)>> << "\n\t";
        });
        // clang-format off
        issue.visit(utl::visitor{
            [&](sema::InvalidDeclaration const& e) {
                std::cout << "Invalid declaration (" << e.reason() << "): ";
                std::cout << std::endl;
            },
            [&](sema::BadTypeConversion const& e) {
                std::cout << "Bad type conversion: ";
                ast::printExpression(e.expression());
                std::cout << std::endl;
                std::cout << "\tFrom " << sym.getName(e.from()) << " to "
                          << sym.getName(e.to()) << "\n";
            },
            [&](sema::BadFunctionCall const& e) {
                std::cout << "Bad function call: " << e.reason() << ": ";
                ast::printExpression(e.expression());
                std::cout << std::endl;
            },
            [&](sema::UseOfUndeclaredIdentifier const& e) {
                std::cout << "Use of undeclared identifier ";
                ast::printExpression(e.expression());
                std::cout << " in scope: " << e.currentScope().name()
                          << std::endl;
            },
            [](issue::ProgramIssueBase const&) { std::cout << std::endl; }
        });
        // clang-format on
        std::cout << std::endl;
    }
}

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

    /// Tokenize the text
    issue::LexicalIssueHandler lexIss;
    auto tokens = lex::lex(text, lexIss);
    printLexicalIssues(lexIss);

    if (!lexIss.empty()) {
        return -1;
    }

    /// Parse the tokens
    issue::SyntaxIssueHandler synIss;
    auto ast = parse::parse(std::move(tokens), synIss);
    printSyntaxIssues(synIss);

    /// Analyse the AST
    issue::SemaIssueHandler semaIss;
    auto symbolTable = sema::analyze(*ast, semaIss);
    printSemaIssues(semaIss, symbolTable);

    /// Generate IR
    ir::Context context;
    auto mod = ast::codegen(*ast, symbolTable, context);

    opt::optimize(context, mod, options.optLevel);

    /// Generate assembly
    auto asmStream = cg::codegen(mod);

    /// Find id of main function
    auto const mainID = [&symbolTable] {
        auto const id  = symbolTable.lookup("main");
        auto const* os = symbolTable.tryGetOverloadSet(id);
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
        std::cout << tfmt::format(tfmt::brightRed,
                                  "No main function defined!\n");
        return -1;
    }

    /// Assemble program
    auto program =
        Asm::assemble(asmStream,
                      { .startFunction =
                            utl::format("main{:x}", mainID.rawValue()) });

    if (options.time) {
        auto const compileEndTime = std::chrono::high_resolution_clock::now();
        auto const dur            = compileEndTime - compileBeginTime;
        std::cout << "Compilation took "
                  << std::chrono::duration_cast<std::chrono::nanoseconds>(dur)
                             .count() /
                         1'000'000.0
                  << "ms\n";
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
