#ifndef SCATHA_SEMA_ANALYSIS_STATEMENTALYSIS_H_
#define SCATHA_SEMA_ANALYSIS_STATEMENTALYSIS_H_

#include "AST/Fwd.h"
#include "Common/Base.h"
#include "Sema/Fwd.h"

namespace scatha::sema {

class SymbolTable;

/// Semantically analyze the statement \p stmt
SCTEST_API void analyzeStatement(AnalysisContext& ctx, ast::Statement* stmt);

} // namespace scatha::sema

#endif // SCATHA_SEMA_ANALYSIS_STATEMENTALYSIS_H_
