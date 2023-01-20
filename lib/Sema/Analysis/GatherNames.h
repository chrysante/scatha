#ifndef SCATHA_SEMA_ANALYSIS_GATHERNAMES_H_
#define SCATHA_SEMA_ANALYSIS_GATHERNAMES_H_

#include "Basic/Basic.h"
#include "Issue/IssueHandler.h"
#include "Sema/Analysis/DependencyGraph.h"

namespace scatha::ast {

class AbstractSyntaxTree;

} // namespace scatha::ast

namespace scatha::sema {

class SymbolTable;

/// In gatherNames phase we declare (but not instantiate) all non-local names in the translation unit, including nested
/// structs and member variables and functions. After executing \p gatherNames() all globally visible symbols are
/// declared in the table, so we can then analyze all e.g. function declarations. With that we build an incomplete
/// dependency graph of the declarations in the program.
///
/// \param sym Symbol table to declare symbols in.
///
/// \param astRoot  Root node of the abstract syntax tree.
///
/// \param issueHandler Handler to write issues to.
///
/// \returns (Incomplete) dependency graph
SCATHA(API)
DependencyGraph gatherNames(SymbolTable& sym, ast::AbstractSyntaxTree& astRoot, issue::SemaIssueHandler& issueHandler);

} // namespace scatha::sema

#endif // SCATHA_SEMA_ANALYSIS_GATHERNAMES_H_
