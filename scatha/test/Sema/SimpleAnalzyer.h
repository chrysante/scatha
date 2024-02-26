#ifndef SCATHA_TEST_SIMPLEANALYZER_H_
#define SCATHA_TEST_SIMPLEANALYZER_H_

#include <string_view>
#include <tuple>

#include "AST/Fwd.h"
#include "Common/UniquePtr.h"
#include "Issue/IssueHandler.h"
#include "Sema/SymbolTable.h"

namespace scatha::test {

std::tuple<UniquePtr<ast::ASTNode>, sema::SymbolTable, IssueHandler>
    produceDecoratedASTAndSymTable(std::span<SourceFile const> sources,
                                   sema::AnalysisOptions const& options = {});

std::tuple<UniquePtr<ast::ASTNode>, sema::SymbolTable, IssueHandler>
    produceDecoratedASTAndSymTable(std::string_view text,
                                   sema::AnalysisOptions const& options = {});

} // namespace scatha::test

#endif // SCATHA_TEST_SIMPLEANALYZER_H_
