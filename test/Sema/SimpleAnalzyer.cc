#include "SimpleAnalzyer.h"

#include "Common/UniquePtr.h"
#include "Issue/IssueHandler.h"
#include "Parser/Lexer.h"
#include "Parser/Parser.h"
#include "Sema/Analyze.h"

namespace scatha::test {

std::tuple<UniquePtr<ast::ASTNode>, sema::SymbolTable, IssueHandler>
    produceDecoratedASTAndSymTable(std::string_view text) {
    IssueHandler issues;
    auto ast = parser::parse(text, issues);
    sema::SymbolTable sym;
    sema::analyze(*ast, sym, issues);
    return { std::move(ast), std::move(sym), std::move(issues) };
}

} // namespace scatha::test
