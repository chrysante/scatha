#ifndef SCATHA_SEMA_ANALYSIS_TYPEINSTANTIATION_H_
#define SCATHA_SEMA_ANALYSIS_TYPEINSTANTIATION_H_

#include <vector>

#include "AST/Fwd.h"
#include "Common/Base.h"
#include "Issue/IssueHandler.h"
#include "Sema/Analysis/StructDependencyGraph.h"
#include "Sema/Fwd.h"

namespace scatha::sema {

/// Creates symbol table entries for all globally visible symbols
/// \Returns a list of all struct types of the program in topsort order, i.e. if
/// `Y` has a member of type `X`, then `X` comes before `Y`
SCATHA_API std::vector<StructureType const*> instantiateEntities(
    Context& context,
    StructDependencyGraph& structDependencies,
    std::span<ast::FunctionDefinition*> functions);

} // namespace scatha::sema

#endif // SCATHA_SEMA_ANALYSIS_TYPEINSTANTIATION_H_
