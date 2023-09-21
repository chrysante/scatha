#include "Sema/SemanticIssuesNEW.h"

#include "AST/AST.h"
#include "Sema/Entity.h"

using namespace scatha;
using namespace sema;

BadStatementNEW::BadStatementNEW(ast::Statement const* statement,
                                 Scope const* scope,
                                 IssueSeverity severity):
    SemaIssue(scope, statement->sourceRange(), severity), stmt(statement) {}

BadDecl::BadDecl(ast::Declaration const* declaration,
                 Scope const* scope,
                 IssueSeverity severity):
    BadStatementNEW(declaration, scope, severity) {}

ast::Declaration const* BadDecl::declaration() const {
    return cast<ast::Declaration const*>(statement());
}

static IssueSeverity toSeverity(BadVarDecl::Reason reason) {
    switch (reason) {
#define SC_SEMA_BADVARDECL_DEF(reason, severity, _)                            \
    case BadVarDecl::reason:                                                   \
        return IssueSeverity::severity;
#include "Sema/SemanticIssuesNEW.def"
    }
}

BadVarDecl::BadVarDecl(ast::VariableDeclaration const* vardecl,
                       Scope const* scope,
                       Reason reason,
                       Type const* type,
                       ast::Expression const* initExpr):
    BadDecl(vardecl, scope, toSeverity(reason)),
    _reason(reason),
    _type(type),
    _initExpr(initExpr) {}

void BadVarDecl::format(std::ostream& str) const {
    switch (reason()) {
#define SC_SEMA_BADVARDECL_DEF(reason, _, message)                             \
    case BadVarDecl::reason:                                                   \
        str << message;                                                        \
        break;
#include "Sema/SemanticIssuesNEW.def"
    }
}

static IssueSeverity toSeverity(BadFuncDef::Reason reason) {
    switch (reason) {
#define SC_SEMA_BADFUNCDEF_DEF(reason, severity, _)                            \
    case BadFuncDef::reason:                                                   \
        return IssueSeverity::severity;
#include "Sema/SemanticIssuesNEW.def"
    }
}

BadExpr::BadExpr(ast::Expression const* expression,
                 Scope const* scope,
                 IssueSeverity severity):
    SemaIssue(scope, expression->sourceRange(), severity), expr(expression) {}
