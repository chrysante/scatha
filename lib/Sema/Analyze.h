#ifndef SCATHA_SEMA_ANALYZE_H_
#define SCATHA_SEMA_ANALYZE_H_

#include <stdexcept>

#include "AST/AST.h"
#include "Issue/IssueHandler.h"
#include "Sema/SymbolTable.h"

namespace scatha::sema {

/// Semantically analyzes and decorates the abstract syntax tree.
///
/// \param root Root of the tree to analyze.
/// \returns The generated symbol table.
SCATHA(API) SymbolTable analyze(ast::AbstractSyntaxTree& root, issue::IssueHandler&);

} // namespace scatha::sema

#endif // SCATHA_SEMA_ANALYZE_H_
