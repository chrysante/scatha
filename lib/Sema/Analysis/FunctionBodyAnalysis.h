#ifndef SCATHA_SEMA_ANALYSIS_FUNCTIONINSTANTIATION_H_
#define SCATHA_SEMA_ANALYSIS_FUNCTIONINSTANTIATION_H_

#include <span>

#include "AST/Fwd.h"
#include "Common/Base.h"
#include "Issue/IssueHandler.h"
#include "Sema/Analysis/StructDependencyGraph.h"

namespace scatha::sema {

class SymbolTable;

/// Semantically analyze the function \p function
SCTEST_API void analyzeFunction(Context& ctx,
                                ast::FunctionDefinition* function);

} // namespace scatha::sema

#endif // SCATHA_SEMA_ANALYSIS_FUNCTIONINSTANTIATION_H_
