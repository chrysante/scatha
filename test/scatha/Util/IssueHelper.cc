#include "Util/IssueHelper.h"

#include <sstream>

#include "Parser/Lexer.h"
#include "Parser/Parser.h"
#include "Sema/SimpleAnalzyer.h"

using namespace scatha;

static void testPrinting(IssueHandler const& iss,
                         std::span<SourceFile const> sources) {
    /// We print all issues to a string that we'll discard to make sure that
    /// printing does not crash
    std::stringstream sstr;
    iss.print(sources, sstr);
}

static void testPrinting(IssueHandler const& iss, std::string_view text) {
    auto source = SourceFile::make(std::string(text));
    testPrinting(iss, std::span(&source, 1));
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

test::IssueHelper test::getSemaIssues(std::span<SourceFile const> sources,
                                      sema::AnalysisOptions const& options) {
    auto [ast, sym, iss] =
        produceDecoratedASTAndSymTable(std::move(sources), options);
    testPrinting(iss, sources);
    return { std::move(iss), std::move(ast), std::move(sym) };
}
