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
    produceDecoratedASTAndSymTable(std::span<SourceFile const> sources,
                                   sema::AnalysisOptions const& options) {
    IssueHandler issues;
    auto ast = parser::parse(sources, issues);
    sema::SymbolTable sym;
    sema::analyze(*ast, sym, issues, options);
    return { std::move(ast), std::move(sym), std::move(issues) };
}

std::tuple<UniquePtr<ast::ASTNode>, sema::SymbolTable, IssueHandler> test::
    produceDecoratedASTAndSymTable(std::string_view text,
                                   sema::AnalysisOptions const& options) {
    auto source = SourceFile::make(std::string(text));
    return produceDecoratedASTAndSymTable(std::span(&source, 1), options);
}
