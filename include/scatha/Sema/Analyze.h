#ifndef SCATHA_SEMA_ANALYZE_H_
#define SCATHA_SEMA_ANALYZE_H_

#include <scatha/AST/Fwd.h>
#include <scatha/Issue/IssueHandler.h>
#include <scatha/Sema/AnalysisResult.h>
#include <scatha/Sema/Fwd.h>

namespace scatha::sema {

/// Semantically analyzes and decorates the abstract syntax tree.
///
/// \param TU Translation unit to analyze
/// \param symbolTable Symbol table to populate.
SCATHA_API AnalysisResult analyze(ast::ASTNode& TU,
                                  SymbolTable& symbolTable,
                                  IssueHandler&,
                                  AnalysisOptions const& options = {});

} // namespace scatha::sema

#endif // SCATHA_SEMA_ANALYZE_H_
