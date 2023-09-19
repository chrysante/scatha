#include "test/Parser/SimpleParser.h"

#include <Parser/Lexer.h>
#include <Parser/Parser.h>

using namespace scatha;

std::pair<UniquePtr<ast::ASTNode>, IssueHandler> test::parse(
    std::string_view text) {
    IssueHandler issues;
    auto ast = ::parser::parse(text, issues);
    return { std::move(ast), std::move(issues) };
}

parser::TokenStream test::makeTokenStream(std::string_view text) {
    IssueHandler issues;
    auto tokens = parser::lex(text, issues);
    return parser::TokenStream(std::move(tokens));
}
