#include "Sema/SemanticIssuesNEW.h"

#include <ostream>
#include <string_view>

#include "AST/AST.h"
#include "Sema/Entity.h"

using namespace scatha;
using namespace sema;

BadStatementNEW::BadStatementNEW(Scope const* scope,
                                 ast::Statement const* statement,
                                 IssueSeverity severity):
    SemaIssue(scope, statement->sourceRange(), severity), stmt(statement) {}

BadDecl::BadDecl(Scope const* scope,
                 ast::Declaration const* declaration,
                 IssueSeverity severity):
    BadStatementNEW(scope, declaration, severity) {}

ast::Declaration const* BadDecl::declaration() const {
    return cast<ast::Declaration const*>(statement());
}

static std::string_view format(ast::Declaration const* decl) {
    using namespace ast;
    using namespace std::literals;
    // clang-format off
    return SC_MATCH (*decl) {
        [](VariableDeclaration const&) { return "Variable declaration"sv; },
        [](ParameterDeclaration const&) { return "Parameter declaration"sv; },
        [](FunctionDefinition const&) { return "Function definition"sv; },
        [](StructDefinition const&) { return "Struct declaration"sv; },
        [](ASTNode const&) -> std::string_view { SC_UNREACHABLE(); },
    }; // clang-format on
}

static std::string_view format(Scope const* scope) {
    using enum ScopeKind;
    switch (scope->kind()) {
    case Global:
        return "";
    case Namespace:
        return "";
    case Variable:
        return "";
    case Function:
        return "";
    case Object:
        return "";
    case Anonymous:
        return "";
    case Invalid:
        return "";
    case _count:
        return "";
    }
}

void DeclInvalidInScope::format(std::ostream& str) const {
    str << ::format(declaration()) << " invalid in " << ::format(scope());
}

static IssueSeverity toSeverity(GenericBadDecl::Reason reason) {
    switch (reason) {
#define SC_SEMA_GENERICBADDECL_DEF(reason, severity, _)                        \
    case GenericBadDecl::reason:                                               \
        return IssueSeverity::severity;
#include "Sema/SemanticIssuesNEW.def"
    }
}

GenericBadDecl::GenericBadDecl(Scope const* scope,
                               ast::Declaration const* declaration,
                               Reason reason):
    BadDecl(scope, declaration, toSeverity(reason)), _reason(reason) {}

static IssueSeverity toSeverity(BadVarDecl::Reason reason) {
    switch (reason) {
#define SC_SEMA_BADVARDECL_DEF(reason, severity, _)                            \
    case BadVarDecl::reason:                                                   \
        return IssueSeverity::severity;
#include "Sema/SemanticIssuesNEW.def"
    }
}

void GenericBadDecl::format(std::ostream& str) const {
    switch (reason()) {
#define SC_SEMA_GENERICBADDECL_DEF(reason, _, message)                         \
    case reason:                                                               \
        str << message;                                                        \
        break;
#include "Sema/SemanticIssuesNEW.def"
    }
}

void Redefinition::format(std::ostream& str) const {
    str << "Redefinition of " << declaration()->name();
}

BadVarDecl::BadVarDecl(Scope const* scope,
                       ast::VariableDeclaration const* vardecl,
                       Reason reason,
                       Type const* type,
                       ast::Expression const* initExpr):
    BadDecl(scope, vardecl, toSeverity(reason)),
    _reason(reason),
    _type(type),
    _initExpr(initExpr) {}

void BadVarDecl::format(std::ostream& str) const {
    switch (reason()) {
#define SC_SEMA_BADVARDECL_DEF(reason, _, message)                             \
    case reason:                                                               \
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

BadExpr::BadExpr(Scope const* scope,
                 ast::Expression const* expression,
                 IssueSeverity severity):
    SemaIssue(scope, expression->sourceRange(), severity), expr(expression) {}
