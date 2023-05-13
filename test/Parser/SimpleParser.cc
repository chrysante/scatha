#include "test/Parser/SimpleParser.h"

#include <Parser/Lexer.h>
#include <Parser/Parser.h>

using namespace scatha;

std::pair<UniquePtr<ast::AbstractSyntaxTree>, IssueHandler> test::parse(
    std::string_view text) {
    IssueHandler issues;
    auto ast = ::parse::parse(text, issues);
    return { std::move(ast), std::move(issues) };
}

parse::TokenStream test::makeTokenStream(std::string_view text) {
    IssueHandler issues;
    auto tokens = parse::lex(text, issues);
    return parse::TokenStream(std::move(tokens));
}
