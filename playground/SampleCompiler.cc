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
#include "CodeGen/IR2ByteCode/CodeGenerator.h"
#include "IR/CFG.h"
#include "IR/Context.h"
#include "IR/Module.h"
#include "IR/Print.h"
#include "IR/Validate.h"
#include "Lexer/Lexer.h"
#include "Lexer/LexicalIssue.h"
#include "Opt/ConstantPropagation.h"
#include "Opt/DCE.h"
#include "Opt/MemToReg.h"
#include "Parser/Parser.h"
#include "Parser/SyntaxIssue.h"
#include "Sema/Analyze.h"
#include "Sema/Print.h"
#include "Sema/SemanticIssue.h"

using namespace scatha;
using namespace scatha::lex;
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
    header(" Tokens ");
    issue::LexicalIssueHandler lexIss;
    auto tokens = lex::lex(text, lexIss);
    if (lexIss.empty()) {
        std::cout << "No lexical issues.\n";
    }
    else {
        std::cout << "Lexical issues:\n";
        for (auto& issue: lexIss.issues()) {
            issue.visit([]<typename T>(T const& iss) {
                std::cout << iss.token().sourceLocation << " " << iss.token()
                          << " : " << utl::nameof<T> << std::endl;
            });
        }
    }

    // Parsing
    header(" AST ");
    issue::SyntaxIssueHandler parseIss;
    auto ast = parse::parse(tokens, parseIss);
    if (parseIss.empty()) {
        std::cout << "No syntax issues.\n";
    }
    else {
        std::cout << "\nEncoutered " << parseIss.issues().size()
                  << " issues:\n";
        for (SyntaxIssue const& issue: parseIss.issues()) {
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
        std::cout << "No semantic issues.\n";
    }
    else {
        std::cout << "\nEncoutered " << semaIss.issues().size() << " issues\n";
        subHeader();
        for (auto const& issue: semaIss.issues()) {
            issue.visit([](auto const& issue) {
                auto const loc = issue.token().sourceLocation;
                std::cout << "Line " << loc.line << " Col " << loc.column
                          << ": ";
                std::cout
                    << utl::nameof<std::decay_t<decltype(issue)>> << "\n\t";
            });
            // clang-format off
            issue.visit(utl::overload{
                [&](sema::InvalidDeclaration const& e) {
                    std::cout << "Invalid declaration ("
                                << e.reason() << "): ";
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
                [](issue::ProgramIssueBase const&) {
                    std::cout << std::endl;
                }
            }); // clang-format on
            std::cout << std::endl;
        }
    }

    if (!lexIss.empty() || !parseIss.empty() || !semaIss.empty()) {
        return;
    }

    subHeader();
    header(" Generated IR ");
    ir::Context irCtx;
    ir::Module mod = ast::codegen(*ast, sym, irCtx);
    ir::print(mod);

    header(" Optimized IR ");
    for (auto& function: mod) {
        opt::memToReg(irCtx, function);
        opt::propagateConstants(irCtx, function);
        opt::dce(irCtx, function);
    }
    ir::print(mod);

    header(" Assembly generated from IR ");
    auto const assembly = cg::codegen_Old(mod);
    print(assembly);

    header(" Assembled Program ");
    /// Start execution with `main` if it exists.
    auto const mainName = [&sym]() -> std::optional<std::string> {
        auto const id  = sym.lookup("main");
        auto const* os = sym.tryGetOverloadSet(id);
        if (!os) {
            return std::nullopt;
        }
        auto const* mainFn = os->find({});
        if (!mainFn) {
            return std::nullopt;
        }
        std::stringstream sstr;
        sstr << "main" << std::hex << mainFn->symbolID();
        return std::move(sstr).str();
    }();
    if (!mainName) {
        std::cout << "No main function defined!\n";
        return;
    }
    auto const program =
        Asm::assemble(assembly, { .startFunction = *mainName });
    svm::print(program.data());
    subHeader();

    svm::VirtualMachine vm;
    vm.loadProgram(program.data());
    vm.execute();
    u64 const exitCode = vm.getState().registers[0];
    std::cout << "VM: Program ended with exit code: [\n\ti: "
              << static_cast<i64>(exitCode) << ", \n\tu: " << exitCode
              << ", \n\tf: " << utl::bit_cast<f64>(exitCode) << "\n]"
              << std::endl;

    subHeader();
}
