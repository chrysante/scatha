#ifndef SCATHA_TEST_SIMPLEANALYZER_H_
#define SCATHA_TEST_SIMPLEANALYZER_H_

#include <string_view>
#include <tuple>

#include "AST/AST.h"
#include "Issue/IssueHandler.h"
#include "Sema/SymbolTable.h"

namespace scatha::test {
	
	std::tuple<
		ast::UniquePtr<ast::AbstractSyntaxTree>,
		sema::SymbolTable,
		issue::IssueHandler
	> produceDecoratedASTAndSymTable(std::string_view text);
	
}

#endif // SCATHA_TEST_SIMPLEANALYZER_H_

