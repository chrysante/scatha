#ifndef SCATHA_SEMA_ANALYSIS_TYPEINSTANTIATION_H_
#define SCATHA_SEMA_ANALYSIS_TYPEINSTANTIATION_H_

#include "AST/AST.h"
#include "Basic/Basic.h"
#include "Issue/IssueHandler.h"
#include "Sema/Analysis/DependencyGraph.h"
#include "Sema/SymbolTable.h"

namespace scatha::sema {

SCATHA(API)
void instantiateEntities(SymbolTable& symbolTable,
                         issue::SemaIssueHandler& issueHandler,
                         DependencyGraph& typeDependencies);

} // namespace scatha::sema

#endif // SCATHA_SEMA_ANALYSIS_TYPEINSTANTIATION_H_
