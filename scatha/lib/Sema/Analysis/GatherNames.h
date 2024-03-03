#ifndef SCATHA_SEMA_ANALYSIS_GATHERNAMES_H_
#define SCATHA_SEMA_ANALYSIS_GATHERNAMES_H_

#include <utl/vector.hpp>

#include "AST/Fwd.h"
#include "Common/Base.h"
#include "Issue/IssueHandler.h"
#include "Sema/Analysis/StructDependencyGraph.h"

namespace scatha::sema {

class SymbolTable;

/// Result structure of `gatherNames()`
/// - `structDependencyGraph` Incomplete struct dependency graph.
///   The edges from data members to their types are still missing at this stage
///   and will be added by `instantiateEntities()`
/// - `functionDefinitions` All function definitions in the program
struct GatherNamesResult {
    StructDependencyGraph structs;
    utl::vector<ast::Declaration*> globals;
};

/// In gatherNames phase we declare (but not instantiate) all global names in
/// the translation unit, including nested structs and member variables and
/// functions. After executing `gatherNames()` all globally visible symbols are
/// declared in the table, so we can then analyze all e.g. function
/// declarations. With that we build an incomplete dependency graph of the
/// declarations in the program.
SCATHA_API GatherNamesResult gatherNames(ast::ASTNode& TU,
                                         AnalysisContext& context);

} // namespace scatha::sema

#endif // SCATHA_SEMA_ANALYSIS_GATHERNAMES_H_
