#ifndef SCATHA_SEMA_ANALYSIS_GATHERNAMES_H_
#define SCATHA_SEMA_ANALYSIS_GATHERNAMES_H_

#include <utl/vector.hpp>

#include "AST/AST.h"
#include "Basic/Basic.h"
#include "Issue/IssueHandler.h"
#include "Sema/Analysis/DependencyGraph.h"
#include "Sema/ObjectType.h"
#include "Sema/SymbolTable.h"

namespace scatha::sema {

/// In gatherNames phase we declare (but not instantiate) all non-local names in the translation unit, including nested structs and member variables and functions.
/// With that we build an incomplete dependency graph of the declarations in the program.
/// \param sym Symbol table to declare symbols in.
/// \param astRoot Root node of the abstract syntax tree.
/// \param issueHandler Handler to write issues to.
/// \returns (Incomplete) dependency graph
utl::vector<DependencyGraphNode> gatherNames(SymbolTable& sym,
                                             ast::AbstractSyntaxTree& astRoot,
                                             issue::IssueHandler& issueHandler);

} // namespace scatha::sema

#endif // SCATHA_SEMA_ANALYSIS_GATHERNAMES_H_

