#include "SimpleAnalzyer.h"

#include "Common/UniquePtr.h"
#include "Issue/IssueHandler.h"
#include "Parser/Lexer.h"
#include "Parser/Parser.h"
#include "Parser/SyntaxIssue.h"
#include "Sema/Analyze.h"
#include "Sema/SemanticIssue.h"

namespace scatha::test {

std::tuple<UniquePtr<ast::AbstractSyntaxTree>,
           sema::SymbolTable,
           issue::SemaIssueHandler>
    produceDecoratedASTAndSymTable(std::string_view text) {
    IssueHandler issues;
    auto tokens = parse::lex(text, issues);
    auto ast    = parse::parse(tokens, issues);
    sema::SymbolTable sym;
    issue::SemaIssueHandler semaIss;
    sema::analyze(*ast, sym, semaIss);
    return { std::move(ast), std::move(sym), std::move(semaIss) };
}

} // namespace scatha::test
