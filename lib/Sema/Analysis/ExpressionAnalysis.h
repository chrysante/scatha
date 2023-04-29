#ifndef SCATHA_SEMA_EXPRESSIONANALYSIS_H_
#define SCATHA_SEMA_EXPRESSIONANALYSIS_H_

#include "AST/AST.h"
#include "Issue/IssueHandler.h"
#include "Sema/SymbolTable.h"

namespace scatha::sema {

class ExpressionAnalysisResult;

/// \param expression The expression to be analyzed.
/// \param symbolTable The symbol table to be read from and written to.
/// \param issueHandler The issue handler to submit issues to. May be null.
/// \returns An `ExpressionAnalysisResult`.
///
bool analyzeExpression(ast::Expression& expression,
                       SymbolTable& symbolTable,
                       IssueHandler& issueHandler);

} // namespace scatha::sema

#endif // SCATHA_SEMA_EXPRESSIONANALYSIS_H_
