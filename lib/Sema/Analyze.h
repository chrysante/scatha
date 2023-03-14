// SCATHA-PUBLIC-HEADER

#ifndef SCATHA_SEMA_ANALYZE_H_
#define SCATHA_SEMA_ANALYZE_H_

#include <scatha/AST/Base.h>
#include <scatha/Issue/IssueHandler.h>
#include <scatha/Sema/SymbolTable.h>

namespace scatha::sema {

/// Semantically analyzes and decorates the abstract syntax tree.
///
/// \param root Root of the tree to analyze.
/// \returns The generated symbol table.
[[nodiscard]] SCATHA(API) SymbolTable
    analyze(ast::AbstractSyntaxTree& root, issue::SemaIssueHandler&);

} // namespace scatha::sema

#endif // SCATHA_SEMA_ANALYZE_H_
