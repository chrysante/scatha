#include "Sema/SemaIssues.h"

#include <ostream>
#include <string_view>

#include <termfmt/termfmt.h>
#include <utl/streammanip.hpp>

#include "AST/AST.h"
#include "Sema/Entity.h"

using namespace scatha;
using namespace sema;
using enum HighlightKind;

static SourceRange getSourceRange(ast::ASTNode const* node) {
    if (node) {
        return node->sourceRange();
    }
    return {};
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
    case Function:
        return "function scope";
    case Type:
        return "struct scope";
    case Invalid:
        return "invalid scope";
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

BadStmt::BadStmt(Scope const* scope,
                 ast::Statement const* statement,
                 IssueSeverity severity):
    SemaIssue(scope, getSourceRange(statement), severity), stmt(statement) {}

static IssueSeverity toSeverity(GenericBadStmt::Reason reason) {
    switch (reason) {
#define SC_SEMA_GENERICBADSTMT_DEF(reason, severity, _)                        \
    case GenericBadStmt::reason:                                               \
        return IssueSeverity::severity;
#include "Sema/SemaIssues.def"
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
#include "Sema/SemaIssues.def"
    }
}

static IssueSeverity toSeverity(BadVarDecl::Reason reason) {
    switch (reason) {
#define SC_SEMA_BADVARDECL_DEF(reason, severity, _)                            \
    case BadVarDecl::reason:                                                   \
        return IssueSeverity::severity;
#include "Sema/SemaIssues.def"
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

BadVarDecl::BadVarDecl(Scope const* _scope,
                       ast::VarDeclBase const* _vardecl,
                       Reason _reason,
                       Type const* _type,
                       ast::Expression const* _initExpr):
    BadDecl(_scope, _vardecl, toSeverity(_reason)),
    _reason(_reason),
    _type(_type),
    _initExpr(_initExpr) {
    switch (reason()) {
#define SC_SEMA_BADVARDECL_DEF(REASON, SEVERITY, MESSAGE)                      \
    case REASON:                                                               \
        MESSAGE;                                                               \
        break;
#include "Sema/SemaIssues.def"
    }
}

void BadVarDecl::format(std::ostream& str) const {
    //    switch (reason()) {
    // #define SC_SEMA_BADVARDECL_DEF(reason, _, message) \
//    case reason: \
//        str << message; \ break;
    // #include "Sema/SemaIssues.def"
    //    }
}

static IssueSeverity toSeverity(BadSMF::Reason reason) {
    switch (reason) {
#define SC_SEMA_BADSMF_DEF(reason, severity, _)                                \
    case BadSMF::reason:                                                       \
        return IssueSeverity::severity;
#include "Sema/SemaIssues.def"
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
#include "Sema/SemaIssues.def"
    }
}

static IssueSeverity toSeverity(BadReturnStmt::Reason reason) {
    switch (reason) {
#define SC_SEMA_BADRETURN_DEF(reason, severity, _)                             \
    case BadReturnStmt::reason:                                                \
        return IssueSeverity::severity;
#include "Sema/SemaIssues.def"
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
#include "Sema/SemaIssues.def"
    }
}

static constexpr utl::streammanip formatRetType =
    [](std::ostream& str, ast::ReturnStatement const* stmt) {
    if (auto* expr = stmt->expression()) {
        str << expr->type()->name();
    }
    else {
        str << "void";
    }
};

static SourceRange getRetSR(ast::ReturnStatement const* stmt) {
    if (auto* expr = stmt->expression()) {
        return expr->sourceRange();
    }
    return stmt->sourceRange();
}

BadReturnTypeDeduction::BadReturnTypeDeduction(
    Scope const* scope,
    ast::ReturnStatement const* stmt,
    ast::ReturnStatement const* confl):
    BadStmt(scope, stmt, IssueSeverity::Error), confl(confl) {
    header([=](std::ostream& str) {
        str << "Conflicting return type deduction in function '"
            << statement()->findAncestor<ast::FunctionDefinition>()->name()
            << "'";
    });
    highlight(Primary, getRetSR(statement()), [=](std::ostream& str) {
        str << "Here return type is deduced as " << formatRetType(statement());
    });
    highlight(Secondary, getRetSR(conflicting()), [=](std::ostream& str) {
        str << "Return type was deduced as " << formatRetType(conflicting());
    });
}

void BadReturnTypeDeduction::format(std::ostream& str) const {
    str << "Function "
        << statement()->findAncestor<ast::FunctionDefinition>()->name()
        << " is here deduced to return " << formatRetType(statement())
        << " but was previously deduced to return "
        << formatRetType(conflicting()) << "\n";
}

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

StructDefCycle::StructDefCycle(Scope const* _scope,
                               std::vector<Entity const*> _cycle):
    BadDecl(_scope, nullptr, IssueSeverity::Error), _cycle(std::move(_cycle)) {
    header("Cyclic struct definition");
    hint("Declare data members as pointers to avoid strong cyclic dependecies");
    for (auto [index, entity]: cycle() | ranges::views::enumerate) {
        highlight(Primary,
                  entity->astNode()->sourceRange(),
                  [=, entity = entity, index = index + 1](std::ostream& str) {
            if (auto* var = dyncast<Variable const*>(entity)) {
                auto* type = cast<Type const*>(cycle()[index % cycle().size()]);
                str << index << ". Member " << var->name() << " of type "
                    << type->name() << " defined here";
            }
            else if (auto* type = dyncast<StructType const*>(entity)) {
                str << index << ". Struct " << type->name() << " defined here";
            }
        });
    }
}

void StructDefCycle::format(std::ostream& str) const {
    str << "Cyclic struct definition\nDefinition cycle is: ";
    for (auto* entity: cycle()) {
        str << formatEntity(entity) << " -> ";
    }
    str << formatEntity(cycle().front());
}

static IssueSeverity toSeverity(BadExpr::Reason reason) {
    switch (reason) {
#define SC_SEMA_BADEXPR_DEF(Type, Reason, Severity, Message)                   \
    case BadExpr::Reason:                                                      \
        return IssueSeverity::Severity;
#include "Sema/SemaIssues.def"
    }
}

BadExpr::BadExpr(Scope const* scope,
                 ast::Expression const* expr,
                 Reason reason):
    SemaIssue(scope, getSourceRange(expr), toSeverity(reason)),
    _reason(reason),
    _expr(expr) {}

BadExpr::BadExpr(Scope const* scope,
                 ast::Expression const* expr,
                 IssueSeverity severity):
    SemaIssue(scope, getSourceRange(expr), severity),
    _reason(BadExprNone),
    _expr(expr) {}

void BadExpr::format(std::ostream& str) const {
    switch (reason()) {
#define SC_SEMA_BADEXPR_DEF(Type, Reason, Severity, Message)                   \
    case Reason: {                                                             \
        [[maybe_unused]] auto* expr = cast<ast::Type const*>(this->expr());    \
        str << Message;                                                        \
        break;                                                                 \
    }
#include "Sema/SemaIssues.def"
    }
}

BadSymRef::BadSymRef(Scope const* scope,
                     ast::Expression const* expr,
                     EntityCategory expected):
    BadExpr(scope, expr, IssueSeverity::Error), _expected(expected) {}

EntityCategory BadSymRef::have() const {
    if (!expr() || !expr()->entity()) {
        return EntityCategory::Indeterminate;
    }
    return expr()->entity()->category();
}

EntityCategory BadSymRef::expected() const { return _expected; }

void BadSymRef::format(std::ostream& str) const {
    using enum EntityCategory;
    switch (expected()) {
    case Value:
        str << "Expected a value";
        break;
    case Type:
        str << "Expected a type";
        break;
    case Indeterminate:
        str << "Internal compiler error (Invalid BadSymRef)";
        break;
    }
}

void BadTypeConv::format(std::ostream& str) const {
    str << "Cannot convert value of type " << expr()->type()->name() << " to "
        << to()->name();
}

void BadValueCatConv::format(std::ostream& str) const {
    str << "Cannot convert " << expr()->valueCategory() << " of type "
        << expr()->type()->name() << " to " << to();
}

void BadMutConv::format(std::ostream& str) const {
    str << "Cannot convert " << expr()->type().mutability() << " value of type "
        << expr()->type()->name() << " to " << to();
}

ORError::ORError(OverloadSet const* os,
                 std::vector<std::pair<QualType, ValueCategory>> argTypes,
                 std::vector<Function const*> matches):
    SemaIssue(nullptr, {}, IssueSeverity::Error),
    os(os),
    argTypes(std::move(argTypes)),
    matches(std::move(matches)) {}

void ORError::format(std::ostream& str) const {
    if (matches.empty()) {
        str << "No matching function to call for " << os->name();
    }
    else {
        str << "Ambiguous function call to " << os->name();
    }
}
