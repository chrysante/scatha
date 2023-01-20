#include "SimpleAnalzyer.h"

#include "Common/UniquePtr.h"
#include "Lexer/Lexer.h"
#include "Parser/Parser.h"
#include "Parser/SyntaxIssue.h"
#include "Sema/Analyze.h"
#include "Sema/SemanticIssue.h"

namespace scatha::test {

std::tuple<UniquePtr<ast::AbstractSyntaxTree>, sema::SymbolTable, issue::SemaIssueHandler>
produceDecoratedASTAndSymTable(std::string_view text) {
    issue::LexicalIssueHandler lexIss;
    auto tokens = lex::lex(text, lexIss);
    issue::SyntaxIssueHandler parseIss;
    auto ast = parse::parse(tokens, parseIss);
    issue::SemaIssueHandler semaIss;
    auto sym = sema::analyze(*ast, semaIss);
    return { std::move(ast), std::move(sym), std::move(semaIss) };
}

} // namespace scatha::test
