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
                                   DTorStack& dtorStack,
                                   AnalysisContext& context);

/// Same as `analyzeExpression()` but ensures that the expression refers to a
/// value
ast::Expression* analyzeValueExpr(ast::Expression* expression,
                                  DTorStack& dtorStack,
                                  AnalysisContext& context);

/// Analyses the expression \p expr and returns the type it refers to. If \p
/// expr does not refer to a type, this function will push an error to the issue
/// handler
Type const* analyzeTypeExpr(ast::Expression* expr, AnalysisContext& context);

} // namespace scatha::sema

#endif // SCATHA_SEMA_EXPRESSIONANALYSIS_H_
