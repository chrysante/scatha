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
#include "Issue/Format.h"

using namespace scatha;
using namespace scatha::lex;
using namespace scatha::parse;

static void printSeparator(utl::format_codes::format_code color, char type, int width = 40) {
    auto& str = std::cout;
    str << (utl::format_codes::_private::_decay(color, true));
    for (int i = 0; i < width; ++i) {
        str.put(type);
    }
    str << (utl::format_codes::_private::_decay(utl::format_codes::reset, true));
    str.put('\n');
}

static void printStrongSeparator(utl::format_codes::format_code color, int width = 40) {
    auto& str = std::cout;
    str.put('\n');
    printSeparator(color, '=');
    str.put('\n');
}

[[gnu::weak]] int main() {
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
        utl::vector<parse::SyntaxIssue> issues(parseIss.issues());
        std::sort(issues.begin(), issues.end(), [](parse::SyntaxIssue const& rhs, parse::SyntaxIssue const& lhs) {
            return rhs.token().sourceLocation < lhs.token().sourceLocation;
        });
        printStrongSeparator(utl::format_codes::light_gray);
        utl::print("{}Encountered syntax issues:\n{}", utl::format_codes::red | utl::format_codes::bold, utl::format_codes::reset);
        StructuredSource const structuredSource(text);
        for (bool first = true; auto const& issue: issues) {
            if (!first) { printSeparator(utl::format_codes::light_gray, '-'); }
            else { first = false; }
            auto const sourceLocation = issue.token().sourceLocation;
            utl::print("{}Error at{}: ", utl::format_codes::red | utl::format_codes::bold, utl::format_codes::reset);
            utl::print("{0}L:{2} C:{3}{1}:\n",
                       utl::format_codes::gray, utl::format_codes::reset,
                       sourceLocation.line, sourceLocation.column);
            issue::highlightToken(structuredSource, issue.token());
            utl::print("{}\n", issue.reason());
            
        }
    }
    printStrongSeparator(utl::format_codes::light_gray);
    ast::printSource(*ast);
}
