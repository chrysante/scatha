#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include <utl/typeinfo.hpp>

#include "AST/PrintSource.h"
#include "AST/PrintTree.h"
#include "AST/PrintExpression.h"
#include "Lexer/Lexer.h"
#include "Parser/SyntaxIssue.h"
#include "Parser/Parser.h"

using namespace scatha;
using namespace scatha::lex;
using namespace scatha::parse;

[[gnu::weak]] int main() {
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
    issue::SyntaxIssueHandler parseIss;
    auto ast = parse::parse(tokens, parseIss);
    if (!parseIss.empty()) {
        std::cout << "Encountered syntax issues:\n";
        
        for (auto& issue: parseIss.issues()) {
            std::cout << "L:" << issue.token().sourceLocation.line << " C:" << issue.token().sourceLocation.column << " : "
            << issue.reason() << std::endl;
        }
    }
    ast::printSource(*ast);
}
