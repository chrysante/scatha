#include "test/IssueHelper.h"

#include "Parser/Lexer.h"
#include "Parser/Parser.h"
#include "test/Sema/SimpleAnalzyer.h"

using namespace scatha;

test::IssueHelper test::getLexicalIssues(std::string_view text) {
    IssueHandler iss;
    (void)parse::lex(text, iss);
    return { std::move(iss) };
}

test::IssueHelper test::getSyntaxIssues(std::string_view text) {
    IssueHandler lexIss;
    auto tokens = parse::lex(text, lexIss);
    IssueHandler synIss;
    auto ast = parse::parse(std::move(tokens), synIss);
    return { std::move(synIss), std::move(ast) };
}

test::IssueHelper test::getSemaIssues(std::string_view text) {
    auto [ast, sym, iss] = produceDecoratedASTAndSymTable(text);
    return { std::move(iss), std::move(ast), std::move(sym) };
}
