#include "SimpleAnalzyer.h"

#include <span>

#include "Common/UniquePtr.h"
#include "Issue/IssueHandler.h"
#include "Parser/Lexer.h"
#include "Parser/Parser.h"
#include "Sema/Analyze.h"

using namespace scatha;
using namespace test;

std::tuple<UniquePtr<ast::ASTNode>, sema::SymbolTable, IssueHandler> test::
    produceDecoratedASTAndSymTable(std::span<SourceFile const> sources) {
    IssueHandler issues;
    auto ast = parser::parse(sources, issues);
    sema::SymbolTable sym;
    sema::analyze(*ast, sym, issues);
    return { std::move(ast), std::move(sym), std::move(issues) };
}

std::tuple<UniquePtr<ast::ASTNode>, sema::SymbolTable, IssueHandler> test::
    produceDecoratedASTAndSymTable(std::string_view text) {
    auto source = SourceFile::make(std::string(text));
    return produceDecoratedASTAndSymTable(std::span(&source, 1));
}
