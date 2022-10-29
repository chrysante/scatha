#include "SimpleAnalzyer.h"

#include "AST/AST.h"
#include "Lexer/Lexer.h"
#include "Parser/Parser.h"
#include "Parser/ParsingIssue.h"
#include "Sema/Analyze.h"
#include "Sema/SemanticIssue.h"

namespace scatha::test {

std::tuple<ast::UniquePtr<ast::AbstractSyntaxTree>, sema::SymbolTable, issue::IssueHandler>
produceDecoratedASTAndSymTable(std::string_view text) {
    issue::IssueHandler iss;
    auto tokens = lex::lex(text, iss);
    parse::Parser p(tokens);
    auto ast = p.parse();
    auto sym = sema::analyze(*ast, iss);
    return { std::move(ast), std::move(sym), std::move(iss) };
}

} // namespace scatha::test
