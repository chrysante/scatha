#ifndef SCATHA_SEMA_ANALYSIS_LIFETIME_H_
#define SCATHA_SEMA_ANALYSIS_LIFETIME_H_

#include <span>

#include "AST/Fwd.h"
#include "Common/UniquePtr.h"
#include "Issue/IssueHandler.h"
#include "Sema/Fwd.h"

namespace scatha::sema {

///
UniquePtr<ast::LifetimeCall> makeConstructorCall(
    sema::ObjectType const* type,
    std::span<UniquePtr<ast::Expression>> arguments,
    SymbolTable& symbolTable,
    IssueHandler& issueHandler,
    SourceRange sourceRange);

///
UniquePtr<ast::LifetimeCall> makeDestructorCall(sema::Object* object);

///
UniquePtr<ast::ExpressionStatement> makeDestructorCallStmt(
    sema::Object* object);

} // namespace scatha::sema

#endif // SCATHA_SEMA_ANALYSIS_LIFETIME_H_
