#include <iostream>
#include <fstream>
#include <filesystem>
#include <string>
#include <sstream>

#include <utl/graph.hpp>
#include <utl/typeinfo.hpp>
#include <utl/ranges.hpp>
#include <utl/vector.hpp>
#include <utl/stdio.hpp>
#include <utl/format.hpp>

#include "AST/PrintTree.h"
#include "AST/PrintSource.h"
#include "Lexer/Lexer.h"
#include "Lexer/LexicalIssue.h"
#include "Parser/Parser.h"
#include "Parser/SyntaxIssue.h"
#include "Sema/Analyze.h"
#include "Sema/PrintSymbolTable.h"

#include "IR/Module.h"
#include "IR/Context.h"
#include "IR/Print.h"
#include "ASTCodeGen/CodeGen.h"

using namespace scatha;

static void sectionHeader(std::string_view header) {
    utl::print("{:=^40}\n", "");
    utl::print("{:=^40}\n", header);
    utl::print("{:=^40}\n", "");
}

// [[gnu::weak]]
int main() {
    auto const filepath = std::filesystem::path(PROJECT_LOCATION) / "playground/Test.sc";
    std::fstream file(filepath);
    if (!file) {
        std::cout << "Failed to open file " << filepath << std::endl;
    }
    
    std::stringstream sstr;
    sstr << file.rdbuf();
    std::string const text = sstr.str();
    
    issue::LexicalIssueHandler lexIss;
    auto tokens = lex::lex(text, lexIss);
    
    if (!lexIss.empty()) {
        sectionHeader(" Encountered lexical issues ");
        for (auto& issue: lexIss.issues()) {
            auto const token = issue.token();
            auto const l = token.sourceLocation;
            std::cout << "Issue at " << token.id << " [L:" << l.line << " C:" << l.column << "] : ";
            issue.visit([]<typename T>(T const&) {
                std::cout << utl::nameof<T> << std::endl;
            });
        }
        return 1;
    }
    else {
        std::cout << "No lexical issues\n";
    }
    
    issue::SyntaxIssueHandler parseIss;
    auto ast = parse::parse(tokens, parseIss);
    
    if (!parseIss.empty()) {
        sectionHeader(" Encountered syntax issues ");
        
        for (auto& issue: parseIss.issues()) {
            std::cout << "L:" << issue.token().sourceLocation.line << " C:" << issue.token().sourceLocation.column << " : "
                      << issue.reason() << std::endl;
        }
        ast::printTree(*ast);
        return 2;
    }
    else {
        std::cout << "No syntax issues\n";
    }
    
    issue::SemaIssueHandler semaIss;
    auto sym = sema::analyze(*ast, semaIss);
    
    if (!semaIss.empty()) {
        sectionHeader(" Encountered semantic issues ");
        for (auto& issue: semaIss.issues()) {
            auto const token = issue.token();
            auto const l = token.sourceLocation;
            std::cout << "Issue at " << token.id << " [L:" << l.line << " C:" << l.column << "] : ";
            issue.visit(utl::visitor{
                [](sema::BadFunctionCall const& badFunctionCall) {
                    std::cout << badFunctionCall.reason() << std::endl;
                },
                [&](sema::BadTypeConversion const& badConversion) {
                    std::cout << "Bad type conversion from "
                              << sym.getName(badConversion.from()) << " to "
                              << sym.getName(badConversion.to()) << std::endl;
                },
                [&](sema::UseOfUndeclaredIdentifier const& uoad) {
                    std::cout << "Use of undeclared identifier \""
                    << uoad.token().id << "\"\n";
                },
                [&](sema::StrongReferenceCycle const& c) {
                    std::cout << "Strong reference cycle: ";
                    for (auto node: c.cycle()) {
                        std::cout << "\"" << node.astNode->token().id << "\" -> ";
                    }
                    std::cout << "\"" << c.cycle().front().astNode->token().id << "\"\n";
                },
                []<typename T>(T const&) { std::cout << utl::nameof<T> << std::endl; }
            });
        }
        return 3;
    }
    else {
        std::cout << "No semantic issues\n";
    }
    
    sectionHeader(" IR Code ");
    
    ir::Context ctx;
    ir::Module mod = ast::codegen(*ast, sym, ctx);
    ir::print(mod);
    
    
}
