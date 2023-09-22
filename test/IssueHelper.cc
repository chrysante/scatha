#include "test/IssueHelper.h"

#include <sstream>

#include "Parser/Lexer.h"
#include "Parser/Parser.h"
#include "test/Sema/SimpleAnalzyer.h"

using namespace scatha;

static void testPrinting(IssueHandler const& iss, std::string_view source) {
    /// We print all issues to a string that we'll discard to make sure that
    /// printing does not crash
    std::stringstream sstr;
    iss.print(source, sstr);
}

test::IssueHelper test::getLexicalIssues(std::string_view source) {
    IssueHandler iss;
    (void)parser::lex(source, iss);
    testPrinting(iss, source);
    return { std::move(iss) };
}

test::IssueHelper test::getSyntaxIssues(std::string_view source) {
    IssueHandler iss;
    auto ast = parser::parse(source, iss);
    testPrinting(iss, source);
    return { std::move(iss), std::move(ast) };
}

test::IssueHelper test::getSemaIssues(std::string_view source) {
    auto [ast, sym, iss] = produceDecoratedASTAndSymTable(source);
    testPrinting(iss, source);
    return { std::move(iss), std::move(ast), std::move(sym) };
}
