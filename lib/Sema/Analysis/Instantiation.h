#ifndef SCATHA_SEMA_ANALYSIS_TYPEINSTANTIATION_H_
#define SCATHA_SEMA_ANALYSIS_TYPEINSTANTIATION_H_

#include "Basic/Basic.h"
#include "Issue/IssueHandler.h"

namespace scatha::sema {

class DependencyGraph;
class SymbolTable;

SCATHA(API)
void instantiateEntities(SymbolTable& symbolTable,
                         issue::SemaIssueHandler& issueHandler,
                         DependencyGraph& typeDependencies);

} // namespace scatha::sema

#endif // SCATHA_SEMA_ANALYSIS_TYPEINSTANTIATION_H_
