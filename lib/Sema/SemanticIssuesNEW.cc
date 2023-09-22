#include "Sema/SemanticIssuesNEW.h"

#include <ostream>
#include <string_view>

#include <utl/streammanip.hpp>

#include "AST/AST.h"
#include "Sema/Entity.h"

using namespace scatha;
using namespace sema;

static SourceRange getSourceRange(ast::Statement const* statement) {
    if (statement) {
        return statement->sourceRange();
    }
    return {};
}

BadStmt::BadStmt(Scope const* scope,
                 ast::Statement const* statement,
                 IssueSeverity severity):
    SemaIssue(scope, getSourceRange(statement), severity), stmt(statement) {}

static IssueSeverity toSeverity(GenericBadStmt::Reason reason) {
    switch (reason) {
#define SC_SEMA_GENERICBADSTMT_DEF(reason, severity, _)                        \
    case GenericBadStmt::reason:                                               \
        return IssueSeverity::severity;
#include "Sema/SemanticIssuesNEW.def"
    }
}

/// Used by `SC_SEMA_GENERICBADSTMT_DEF`
static std::string_view format(ast::Statement const* stmt) {
    using namespace ast;
    using namespace std::literals;
    // clang-format off
    return SC_MATCH (*stmt) {
        [](VariableDeclaration const&) { return "Variable declaration"sv; },
        [](ParameterDeclaration const&) { return "Parameter declaration"sv; },
        [](FunctionDefinition const&) { return "Function definition"sv; },
        [](StructDefinition const&) { return "Struct declaration"sv; },
        [](ReturnStatement const&) { return "Return statement"sv; },
        [](Statement const&) { return "Statement"sv; },
    }; // clang-format on
}

/// Used by `SC_SEMA_GENERICBADSTMT_DEF`
static std::string_view format(Scope const* scope) {
    using enum ScopeKind;
    switch (scope->kind()) {
    case Global:
        return "global scope";
    case Namespace:
        return "namespace scope";
    case Variable:
#warning
        return "???";
    case Function:
        return "function scope";
    case Type:
        return "struct scope";
    case Invalid:
        return "invalid scope";
    }
}

GenericBadStmt::GenericBadStmt(Scope const* scope,
                               ast::Statement const* stmt,
                               Reason reason):
    BadStmt(scope, stmt, toSeverity(reason)), _reason(reason) {}

void GenericBadStmt::format(std::ostream& str) const {
    switch (reason()) {
#define SC_SEMA_GENERICBADSTMT_DEF(reason, _, message)                         \
    case reason:                                                               \
        str << message;                                                        \
        break;
#include "Sema/SemanticIssuesNEW.def"
    }
}

static IssueSeverity toSeverity(BadVarDecl::Reason reason) {
    switch (reason) {
#define SC_SEMA_BADVARDECL_DEF(reason, severity, _)                            \
    case BadVarDecl::reason:                                                   \
        return IssueSeverity::severity;
#include "Sema/SemanticIssuesNEW.def"
    }
}

BadDecl::BadDecl(Scope const* scope,
                 ast::Declaration const* declaration,
                 IssueSeverity severity):
    BadStmt(scope, declaration, severity) {}

void Redefinition::format(std::ostream& str) const {
    str << "Redefinition";
    if (declaration()) {
        str << " of " << declaration()->name();
    }
}

BadVarDecl::BadVarDecl(Scope const* scope,
                       ast::VarDeclBase const* vardecl,
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

static IssueSeverity toSeverity(BadSMF::Reason reason) {
    switch (reason) {
#define SC_SEMA_BADSMF_DEF(reason, severity, _)                                \
    case BadSMF::reason:                                                       \
        return IssueSeverity::severity;
#include "Sema/SemanticIssuesNEW.def"
    }
}

BadSMF::BadSMF(Scope const* scope,
               ast::FunctionDefinition const* funcdef,
               Reason reason,
               SpecialMemberFunction SMF,
               StructType const* parent):
    BadDecl(scope, funcdef, toSeverity(reason)),
    _reason(reason),
    smf(SMF),
    _parent(parent) {}

void BadSMF::format(std::ostream& str) const {
    switch (reason()) {
#define SC_SEMA_BADSMF_DEF(reason, _, message)                                 \
    case reason:                                                               \
        str << message;                                                        \
        break;
#include "Sema/SemanticIssuesNEW.def"
    }
}

static IssueSeverity toSeverity(BadReturnStmt::Reason reason) {
    switch (reason) {
#define SC_SEMA_BADRETURN_DEF(reason, severity, _)                             \
    case BadReturnStmt::reason:                                                \
        return IssueSeverity::severity;
#include "Sema/SemanticIssuesNEW.def"
    }
}

BadReturnStmt::BadReturnStmt(Scope const* scope,
                             ast::ReturnStatement const* statement,
                             Reason reason):
    BadStmt(scope, statement, toSeverity(reason)), _reason(reason) {}

void BadReturnStmt::format(std::ostream& str) const {
    switch (reason()) {
#define SC_SEMA_BADRETURN_DEF(reason, _, message)                              \
    case reason:                                                               \
        str << message;                                                        \
        break;
#include "Sema/SemanticIssuesNEW.def"
    }
}

StructDefCycle::StructDefCycle(Scope const* scope,
                               std::vector<Entity const*> cycle):
    BadDecl(scope, nullptr, IssueSeverity::Error), _cycle(std::move(cycle)) {}

static constexpr utl::streammanip formatEntity =
    [](std::ostream& str, Entity const* e) {
    // clang-format off
    SC_MATCH (*e) {
        [&](StructType const&) { str << "struct"; },
        [&](Variable const& var) { str << (var.isMut() ? "var" : "let"); },
        [&](Entity const&) { str << "<unknown-decl>"; },
    }; // clang-format on
    str << " " << e->name();
};

void StructDefCycle::format(std::ostream& str) const {
    str << "Cyclic struct definition\nDefinition cycle is: ";
    for (auto* entity: cycle()) {
        str << formatEntity(entity) << " -> ";
    }
    str << formatEntity(cycle().front());
}

BadExpr::BadExpr(Scope const* scope,
                 ast::Expression const* expression,
                 IssueSeverity severity):
    SemaIssue(scope, expression->sourceRange(), severity), expr(expression) {}

static IssueSeverity toSeverity(BadIdentifier::Reason reason) {
    switch (reason) {
#define SC_SEMA_BADID_DEF(reason, severity, _)                                 \
    case BadIdentifier::reason:                                                \
        return IssueSeverity::severity;
#include "Sema/SemanticIssuesNEW.def"
    }
}

BadIdentifier::BadIdentifier(Scope const* scope,
                             ast::Identifier const* id,
                             Reason reason):
    BadExpr(scope, id, toSeverity(reason)), _reason(reason) {}

void BadIdentifier::format(std::ostream& str) const {
    switch (reason()) {
#define SC_SEMA_BADID_DEF(reason, _, message)                                  \
    case reason:                                                               \
        str << message;                                                        \
        break;
#include "Sema/SemanticIssuesNEW.def"
    }
}

static IssueSeverity toSeverity(BadUnaryExpr::Reason reason) {
    switch (reason) {
#define SC_SEMA_BADUNEXPR_DEF(reason, severity, _)                             \
    case BadUnaryExpr::reason:                                                 \
        return IssueSeverity::severity;
#include "Sema/SemanticIssuesNEW.def"
    }
}

BadUnaryExpr::BadUnaryExpr(Scope const* scope,
                           ast::UnaryExpression const* expr,
                           Reason reason):
    BadExpr(scope, expr, toSeverity(reason)), _reason(reason) {}

void BadUnaryExpr::format(std::ostream& str) const {
    switch (reason()) {
#define SC_SEMA_BADUNEXPR_DEF(reason, _, message)                              \
    case reason:                                                               \
        str << message;                                                        \
        break;
#include "Sema/SemanticIssuesNEW.def"
    }
}

static IssueSeverity toSeverity(BadBinaryExpr::Reason reason) {
    switch (reason) {
#define SC_SEMA_BADBINEXPR_DEF(reason, severity, _)                            \
    case BadBinaryExpr::reason:                                                \
        return IssueSeverity::severity;
#include "Sema/SemanticIssuesNEW.def"
    }
}

static std::string_view format(ast::BinaryOperator op) {
    using enum ast::BinaryOperator;
    switch (op) {
    case Multiplication:
        return "Multiplication";
    case Division:
        return "Division";
    case Remainder:
        return "Remainder operation";
    case Addition:
        return "Addition";
    case Subtraction:
        return "Subtraction";
    case LeftShift:
        return "Left shift";
    case RightShift:
        return "Right shift";
    case Less:
        [[fallthrough]];
    case LessEq:
        [[fallthrough]];
    case Greater:
        [[fallthrough]];
    case GreaterEq:
        return "Relational comparison";
    case Equals:
        [[fallthrough]];
    case NotEquals:
        return "Equality comparison";
    case BitwiseAnd:
        return "Bitwise and";
    case BitwiseXOr:
        return "Bitwise xor";
    case BitwiseOr:
        return "Bitwise or";
    case LogicalAnd:
        return "Logical and";
    case LogicalOr:
        return "Logical and";
    case Assignment:
        return "Assignment";
    case AddAssignment:
        [[fallthrough]];
    case SubAssignment:
        [[fallthrough]];
    case MulAssignment:
        [[fallthrough]];
    case DivAssignment:
        [[fallthrough]];
    case RemAssignment:
        [[fallthrough]];
    case LSAssignment:
        [[fallthrough]];
    case RSAssignment:
        [[fallthrough]];
    case AndAssignment:
        [[fallthrough]];
    case OrAssignment:
        [[fallthrough]];
    case XOrAssignment:
        return "Arithmetic assignment";
    case Comma:
        return "Comma operation";
    }
}

BadBinaryExpr::BadBinaryExpr(Scope const* scope,
                             ast::BinaryExpression const* expr,
                             Reason reason):
    BadExpr(scope, expr, toSeverity(reason)), _reason(reason) {}

void BadBinaryExpr::format(std::ostream& str) const {
    switch (reason()) {
#define SC_SEMA_BADBINEXPR_DEF(reason, _, message)                             \
    case reason:                                                               \
        str << message;                                                        \
        break;
#include "Sema/SemanticIssuesNEW.def"
    }
}

static IssueSeverity toSeverity(BadMemAcc::Reason reason) {
    switch (reason) {
#define SC_SEMA_BADMEMACC_DEF(reason, severity, _)                             \
    case BadMemAcc::reason:                                                    \
        return IssueSeverity::severity;
#include "Sema/SemanticIssuesNEW.def"
    }
}

BadMemAcc::BadMemAcc(Scope const* scope,
                     ast::MemberAccess const* expr,
                     Reason reason):
    BadExpr(scope, expr, toSeverity(reason)), _reason(reason) {}

void BadMemAcc::format(std::ostream& str) const {
    switch (reason()) {
#define SC_SEMA_BADMEMACC_DEF(reason, _, message)                              \
    case reason:                                                               \
        str << message;                                                        \
        break;
#include "Sema/SemanticIssuesNEW.def"
    }
}
