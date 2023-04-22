// SCATHA-PUBLIC-HEADER

#ifndef SCATHA_SEMA_ANALYZE_H_
#define SCATHA_SEMA_ANALYZE_H_

#include <scatha/AST/Fwd.h>
#include <scatha/Issue/IssueHandler.h>
#include <scatha/Sema/SymbolTable.h>

namespace scatha::sema {

/// Semantically analyzes and decorates the abstract syntax tree.
///
/// \param root Root of the tree to analyze.
/// \param symbolTable Symbol table to populate.
SCATHA_API void analyze(ast::AbstractSyntaxTree& root,
                        SymbolTable& symbolTable,
                        IssueHandler&);

} // namespace scatha::sema

#endif // SCATHA_SEMA_ANALYZE_H_
