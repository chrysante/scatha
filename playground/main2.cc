#include <iostream>
#include <fstream>
#include <filesystem>
#include <string>
#include <sstream>

#include <utl/graph.hpp>
#include <utl/ranges.hpp>
#include <utl/vector.hpp>

#include "Lexer/Lexer.h"
#include "Parser/Parser.h"
#include "Sema/Analyze.h"
#include "Sema/PrintSymbolTable.h"

using namespace scatha;

int main() {
    auto const filepath = std::filesystem::path(PROJECT_LOCATION) / "playground/Test.sc";
    std::fstream file(filepath);
    if (!file) {
        std::cout << "Failed to open file " << filepath << std::endl;
    }
    
    std::stringstream sstr;
    sstr << file.rdbuf();
    std::string const text = sstr.str();
    
    auto ast = [&]{
        try {
            lex::Lexer l(text);
            auto tokens = l.lex();
            parse::Parser p(tokens);
            return p.parse();
        }
        catch (std::exception const& e) {
            std::cout << e.what() << std::endl;
            std::exit(0);
        }
    }();
    
    issue::IssueHandler iss;
    auto sym = sema::analyze(*ast, iss);
    sema::printSymbolTable(sym);
    
    if (!iss.empty()) {
        for (auto& issue: iss.semaIssues()) {
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
                [](auto&) { std::cout << std::endl; }
            });
        }
        return 0;
    }
    
}
