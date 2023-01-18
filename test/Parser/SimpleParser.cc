#include "test/Parser/SimpleParser.h"

#include <Lexer/Lexer.h>
#include <Parser/Parser.h>

using namespace scatha;

std::pair<UniquePtr<ast::AbstractSyntaxTree>, issue::SyntaxIssueHandler> test::parse(std::string_view text) {
    issue::LexicalIssueHandler lexIss;
    auto tokens = lex::lex(text, lexIss);
    issue::SyntaxIssueHandler syntaxIss;
    auto ast = ::parse::parse(std::move(tokens), syntaxIss);
    return { std::move(ast), std::move(syntaxIss) };
}

parse::TokenStream test::makeTokenStream(std::string_view text) {
    issue::LexicalIssueHandler iss;
    auto tokens = lex::lex(text, iss);
    return parse::TokenStream(std::move(tokens));
}
