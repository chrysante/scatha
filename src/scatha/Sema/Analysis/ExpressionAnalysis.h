#ifndef SCATHA_SEMA_EXPRESSIONANALYSIS_H_
#define SCATHA_SEMA_EXPRESSIONANALYSIS_H_

#include "AST/Fwd.h"
#include "Issue/IssueHandler.h"
#include "Sema/Fwd.h"

namespace scatha::sema {

class ExpressionAnalysisResult;

/// Semantically analyses the expression \p expression and decorates all AST
/// nodes.
/// Creates entries in the symbol table for defined entities.
ast::Expression* analyzeExpression(ast::Expression* expression,
                                   CleanupStack& cleanupStack,
                                   AnalysisContext& context);

/// Same as `analyzeExpression()` but ensures that the expression refers to a
/// value
ast::Expression* analyzeValueExpr(ast::Expression* expression,
                                  CleanupStack& cleanupStack,
                                  AnalysisContext& context);

/// Analyses the expression \p expr and returns the type or deduction qualifier
/// it refers to.
/// If \p expr does not refer to a type or deduction qualifier, this function
/// will push an error to the issue handler
ast::Expression* analyzeTypeExpr(ast::Expression* expr,
                                 AnalysisContext& context);

/// Deduces the target type of the value expression \p valueExpr based on the
/// type expression or deduction qualifier expression \p typeExpr
/// Returns null and submits errors if type deduction is not possible.
/// \Note Even if this function succeeds it is not guaranteed that a conversion
/// from \p valueExpr to the target type is possible.
Type const* deduceType(ast::Expression* typeExpr, ast::Expression* valueExpr,
                       AnalysisContext& context);

} // namespace scatha::sema

#endif // SCATHA_SEMA_EXPRESSIONANALYSIS_H_
