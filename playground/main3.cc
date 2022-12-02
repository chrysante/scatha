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
#include "Lexer/Lexer.h"
#include "Parser/SyntaxIssue.h"
#include "Parser/Parser.h"

using namespace scatha;
using namespace scatha::lex;
using namespace scatha::parse;

static void printSeparator(utl::format_codes::format_code color, int width = 40) {
    utl::print("\n{2}{1:=<{0}}{3}\n\n", width, "", color, utl::format_codes::reset);
}

int main() {
    auto const filepath = std::filesystem::path(PROJECT_LOCATION) / "playground/Test.sc";
    std::fstream file(filepath);
    if (!file) {
        std::cout << "Failed to open file " << filepath << std::endl;
        return 0;
    }
    
    std::stringstream sstr;
    sstr << file.rdbuf();
    std::string const text = sstr.str();
    
    issue::LexicalIssueHandler lexIss;
    auto tokens = lex::lex(text, lexIss);
    issue::SyntaxIssueHandler parseIss;
    auto ast = parse::parse(tokens, parseIss);
    if (!parseIss.empty()) {
        printSeparator(utl::format_codes::light_gray);
        utl::print("{}Encountered syntax issues:\n{}", utl::format_codes::red | utl::format_codes::bold, utl::format_codes::reset);
        for (auto& issue: parseIss.issues()) {
            auto const sourceLocation = issue.token().sourceLocation;
            utl::print("{0}L:{2:>4} C:{3:>3}{1} : {4}\n",
                       utl::format_codes::gray, utl::format_codes::reset,
                       sourceLocation.line, sourceLocation.column, issue.reason());
//            std::cout << "L:" << issue.token().sourceLocation.line << " C:" << issue.token().sourceLocation.column << " : "
//            << issue.reason() << std::endl;
        }
    }
    printSeparator(utl::format_codes::light_gray);
    ast::printSource(*ast);
}
