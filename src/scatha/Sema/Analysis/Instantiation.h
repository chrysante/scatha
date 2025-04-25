#ifndef SCATHA_SEMA_ANALYSIS_INSTANTIATION_H_
#define SCATHA_SEMA_ANALYSIS_INSTANTIATION_H_

#include <vector>

#include "AST/Fwd.h"
#include "Common/Base.h"
#include "Issue/IssueHandler.h"
#include "Sema/Analysis/StructDependencyGraph.h"
#include "Sema/Fwd.h"

namespace scatha::sema {

/// Creates symbol table entries for all globally visible symbols
/// \Returns a list of all record types of the program in topsort order, i.e. if
/// `Y` has a member of type `X`, then `X` comes before `Y`
SCATHA_API std::vector<RecordType const*> instantiateEntities(
    AnalysisContext& context, StructDependencyGraph& structDependencies,
    std::span<ast::Declaration*> globals);

} // namespace scatha::sema

#endif // SCATHA_SEMA_ANALYSIS_INSTANTIATION_H_
