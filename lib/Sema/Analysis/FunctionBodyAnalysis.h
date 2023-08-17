#ifndef SCATHA_SEMA_ANALYSIS_FUNCTIONINSTANTIATION_H_
#define SCATHA_SEMA_ANALYSIS_FUNCTIONINSTANTIATION_H_

#include <span>

#include "Common/Base.h"
#include "Issue/IssueHandler.h"
#include "Sema/Analysis/DependencyGraph.h"

namespace scatha::sema {

class SymbolTable;

/// Instantiate all functions in the program.
/// Here we don't need the dependency graph anymore, as functions don't strongly
/// depend on each other at compile time. This may change if we introduce
/// compile time evaluation of functions.
SCATHA_API void analyzeFunctionBodies(
    SymbolTable& symbolTable,
    IssueHandler& issueHandler,
    std::span<DependencyGraphNode const> functions);

} // namespace scatha::sema

#endif // SCATHA_SEMA_ANALYSIS_FUNCTIONINSTANTIATION_H_
