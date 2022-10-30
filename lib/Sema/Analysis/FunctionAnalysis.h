#ifndef SCATHA_SEMA_ANALYSIS_FUNCTIONINSTANTIATION_H_
#define SCATHA_SEMA_ANALYSIS_FUNCTIONINSTANTIATION_H_

#include <span>

#include "AST/AST.h"
#include "Basic/Basic.h"
#include "Issue/IssueHandler.h"
#include "Sema/Analysis/DependencyGraph.h"
#include "Sema/SymbolTable.h"

namespace scatha::sema {

/// Instantiate all functions in the program.
/// Here we don't need the dependency graph anymore, as functions don't strongly depend on each other at compile time.
/// This may change if we introduce compile time evaluation of functions.
SCATHA(API)
void analyzeFunctions(SymbolTable& symbolTable,
                      issue::SemaIssueHandler& issueHandler,
                      std::span<DependencyGraphNode const> functions);

} // namespace scatha::sema

#endif // SCATHA_SEMA_ANALYSIS_FUNCTIONINSTANTIATION_H_
