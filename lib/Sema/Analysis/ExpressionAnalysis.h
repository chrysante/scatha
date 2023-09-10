#ifndef SCATHA_SEMA_EXPRESSIONANALYSIS_H_
#define SCATHA_SEMA_EXPRESSIONANALYSIS_H_

#include "AST/Fwd.h"
#include "Issue/IssueHandler.h"
#include "Sema/Analysis/DTorStack.h"
#include "Sema/Fwd.h"

namespace scatha::sema {

class ExpressionAnalysisResult;

/// Semantically analyses the expression \p expression and decorates all AST
/// nodes.
/// Creates entries in the symbol table for defined entities.
ast::Expression* analyzeExpression(ast::Expression* expression,
                                   DTorStack& dtorStack,
                                   Context& context);

} // namespace scatha::sema

#endif // SCATHA_SEMA_EXPRESSIONANALYSIS_H_
