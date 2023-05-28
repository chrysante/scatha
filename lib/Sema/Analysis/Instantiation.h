#ifndef SCATHA_SEMA_ANALYSIS_TYPEINSTANTIATION_H_
#define SCATHA_SEMA_ANALYSIS_TYPEINSTANTIATION_H_

#include "Common/Base.h"
#include "Issue/IssueHandler.h"

namespace scatha::sema {

class DependencyGraph;
class StructureType;
class SymbolTable;

SCATHA_API std::vector<StructureType const*> instantiateEntities(
    SymbolTable& symbolTable,
    IssueHandler& issueHandler,
    DependencyGraph& typeDependencies);

} // namespace scatha::sema

#endif // SCATHA_SEMA_ANALYSIS_TYPEINSTANTIATION_H_
