#include "test/IssueHelper.h"

#include "Lexer/Lexer.h"
#include "Parser/Parser.h"
#include "test/Sema/SimpleAnalzyer.h"

using namespace scatha;

test::LexicalIssueHelper test::getLexicalIssues(std::string_view text) {
    issue::LexicalIssueHandler iss;
    (void)lex::lex(text, iss);
    return { std::move(iss) };
}

test::SyntaxIssueHelper test::getSyntaxIssues(std::string_view text) {
    issue::LexicalIssueHandler lexIss;
    auto tokens = lex::lex(text, lexIss);
    issue::SyntaxIssueHandler synIss;
    auto ast = parse::parse(std::move(tokens), synIss);
    return { std::move(synIss), std::move(ast) };
}

test::SemaIssueHelper test::getSemaIssues(std::string_view text) {
    auto [ast, sym, iss] = produceDecoratedASTAndSymTable(text);
    return { std::move(iss), std::move(ast), std::move(sym) };
}
