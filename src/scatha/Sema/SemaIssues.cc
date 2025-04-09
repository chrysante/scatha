#include "Sema/SemaIssues.h"

#include <ostream>
#include <string_view>

#include <range/v3/algorithm.hpp>
#include <termfmt/termfmt.h>
#include <utl/strcat.hpp>
#include <utl/streammanip.hpp>

#include "AST/AST.h"
#include "Sema/Entity.h"
#include "Sema/Format.h"

using namespace scatha;
using namespace sema;
using namespace tfmt::modifiers;
using enum HighlightKind;

static SourceRange getSourceRange(ast::ASTNode const* node) {
    if (node) {
        return node->sourceRange();
    }
    return {};
}

static SourceRange getSourceRange(
    std::convertible_to<ast::ASTNode const*> auto... node)
    requires(sizeof...(node) > 1)
{
    return merge(getSourceRange(node)...);
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

static std::string formatOrdinal(size_t index) {
    if (index < 9) {
        static std::string const Small[9] = {
            "first", "second",  "third",  "forth", "fifth",
            "sixth", "seventh", "eighth", "ninth",
        };
        return Small[index];
    }
    if (index < 20) {
        return utl::strcat(index + 1, "th");
    }
    switch (index % 10) {
    case 0:
        return utl::strcat(index + 1, "st");
    case 1:
        return utl::strcat(index + 1, "nd");
    case 2:
        return utl::strcat(index + 1, "rd");
    default:
        return utl::strcat(index + 1, "th");
    }
}

/// Formats the given arguments with a preceding "a" or "an" depending on the
/// first letter on the string obtained by formatting \p args...
static std::string formatWithIndefArticle(auto const&... args) {
    auto text = utl::strcat(args...);
    if (text.empty()) {
        return {};
    }
    if (ranges::contains("aeiou", text.front())) {
        return utl::strcat("an ", text);
    }
    return utl::strcat("a ", text);
}

template <typename T>
static T singularPlural(std::integral auto count, T singular, T plural) {
    return count == 1 ? singular : plural;
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
    SC_UNREACHABLE();
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
    SC_UNREACHABLE();
}

BadStmt::BadStmt(Scope const* scope, ast::Statement const* statement,
                 IssueSeverity severity):
    SemaIssue(scope, getSourceRange(statement), severity), stmt(statement) {}

static IssueSeverity toSeverity(GenericBadStmt::Reason reason) {
    switch (reason) {
#define SC_SEMA_GENERICBADSTMT_DEF(reason, severity, _)                        \
    case GenericBadStmt::reason:                                               \
        return IssueSeverity::severity;
#include "Sema/SemaIssues.def.h"
    }
    SC_UNREACHABLE();
}

GenericBadStmt::GenericBadStmt(Scope const* scope, ast::Statement const* stmt,
                               Reason reason):
    BadStmt(scope, stmt, toSeverity(reason)), _reason(reason) {}

void GenericBadStmt::format(std::ostream& str) const {
    switch (reason()) {
#define SC_SEMA_GENERICBADSTMT_DEF(reason, _, message)                         \
    case reason:                                                               \
        str << message;                                                        \
        break;
#include "Sema/SemaIssues.def.h"
    }
}

static ast::ImportStatement const* getImportStmt(ast::ASTNode const* node) {
    if (!node) {
        return nullptr;
    }
    if (auto* stmt = dyncast<ast::ImportStatement const*>(node)) {
        return stmt;
    }
    return node->findAncestor<ast::ImportStatement>();
}

BadImport::BadImport(Scope const* scope, ast::ASTNode const* node,
                     Reason reason):
    BadStmt(scope, getImportStmt(node), IssueSeverity::Error), _reason(reason) {
    switch (reason) {
    case LibraryNotFound:
        primary(getSourceRange(node),
                [=](std::ostream& str) { str << "Cannot find library"; });
        break;
    case InvalidExpression:
        primary(getSourceRange(node),
                [=](std::ostream& str) { str << "Invalid import expression"; });
        break;
    case UnscopedForeignLibImport:
        primary(getSourceRange(node), [=](std::ostream& str) {
            str << "Must use 'import' to import foreign library";
        });
        break;
    }
}

BadImport::BadImport(Scope const* scope, std::string name):
    BadStmt(scope, nullptr, IssueSeverity::Error),
    _reason(LibraryNotFound),
    name(std::move(name)) {}

void BadImport::format(std::ostream& str) const {
    str << "Cannot find library \"" << name << "\"";
}

static IssueSeverity toSeverity(BadVarDecl::Reason reason) {
    switch (reason) {
#define SC_SEMA_BADVARDECL_DEF(reason, severity, _)                            \
    case BadVarDecl::reason:                                                   \
        return IssueSeverity::severity;
#include "Sema/SemaIssues.def.h"
    }
    SC_UNREACHABLE();
}

BadDecl::BadDecl(Scope const* scope, ast::Declaration const* declaration,
                 IssueSeverity severity):
    BadStmt(scope, declaration, severity) {}

Redefinition::Redefinition(Scope const* scope,
                           ast::Declaration const* declaration,
                           Entity const* existing):
    BadDecl(scope, declaration, IssueSeverity::Error), _existing(existing) {
    primary(sourceRange(), [=](std::ostream& str) {
        str << "Redefinition";
        if (declaration) {
            str << " of " << declaration->name();
        }
    });
    secondary(getSourceRange(existing->astNode()), [=](std::ostream& str) {
        str << "Existing declaration is here";
    });
}

BadVarDecl::BadVarDecl(Scope const* _scope, ast::VarDeclBase const* _vardecl,
                       Reason _reason, Type const* _type,
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
#include "Sema/SemaIssues.def.h"
    }
}

static IssueSeverity toSeverity(BadBaseDecl::Reason reason) {
    switch (reason) {
#define SC_SEMA_BADBASEDECL_DEF(reason, severity, _)                           \
    case BadBaseDecl::reason:                                                  \
        return IssueSeverity::severity;
#include "Sema/SemaIssues.def.h"
    }
    SC_UNREACHABLE();
}

BadBaseDecl::BadBaseDecl(Scope const* _scope,
                         ast::BaseClassDeclaration const* _decl, Reason _reason,
                         Type const* _type):
    BadDecl(_scope, _decl, toSeverity(_reason)),
    _reason(_reason),
    _type(_type) {
    switch (reason()) {
#define SC_SEMA_BADBASEDECL_DEF(REASON, SEVERITY, MESSAGE)                     \
    case REASON:                                                               \
        MESSAGE;                                                               \
        break;
#include "Sema/SemaIssues.def.h"
    }
}

static IssueSeverity toSeverity(BadFuncDef::Reason reason) {
    switch (reason) {
#define SC_SEMA_BADFUNCDEF_DEF(reason, severity, ...)                          \
    case BadFuncDef::reason:                                                   \
        return IssueSeverity::severity;
#include "Sema/SemaIssues.def.h"
    }
    SC_UNREACHABLE();
}

BadFuncDef::BadFuncDef(Scope const* scope,
                       ast::FunctionDefinition const* funcdef, Reason reason):
    BadFuncDef(InitAsBase{}, scope, funcdef, toSeverity(reason), reason) {}

BadFuncDef::BadFuncDef(InitAsBase, Scope const* scope,
                       ast::FunctionDefinition const* funcdef,
                       IssueSeverity severity, Reason reason):
    BadDecl(scope, funcdef, severity), _reason(reason) {
    switch (reason) {
#define SC_SEMA_BADFUNCDEF_DEF(REASON, SEVERITY, _, MESSAGE)                   \
    case REASON:                                                               \
        MESSAGE;                                                               \
        break;
#include "Sema/SemaIssues.def.h"
    }
}

std::string_view BadFuncDef::name() const { return definition()->name(); }

void BadFuncDef::format(std::ostream& str) const {
    switch (reason()) {
#define SC_SEMA_BADFUNCDEF_DEF(reason, _, message, newMessage)                 \
    case reason:                                                               \
        str << message;                                                        \
        break;
#include "Sema/SemaIssues.def.h"
    }
}

static IssueSeverity toSeverity(BadSMF::Reason reason) {
    switch (reason) {
#define SC_SEMA_BADSMF_DEF(reason, severity, _)                                \
    case BadSMF::reason:                                                       \
        return IssueSeverity::severity;
#include "Sema/SemaIssues.def.h"
    }
    SC_UNREACHABLE();
}

BadSMF::BadSMF(Scope const* scope, ast::FunctionDefinition const* funcdef,
               Reason reason, StructType const* parent):
    BadFuncDef(InitAsBase{}, scope, funcdef, toSeverity(reason)),
    _reason(reason),
    _parent(parent) {}

void BadSMF::format(std::ostream& str) const {
    switch (reason()) {
#define SC_SEMA_BADSMF_DEF(reason, _, message)                                 \
    case reason:                                                               \
        str << message;                                                        \
        break;
#include "Sema/SemaIssues.def.h"
    }
}

static utl::streammanip badAccCtrlMessage =
    [](std::ostream& str, AccessControl have, AccessControl expected,
       auto becauseOf) {
    str << "Access control '" << have << "' is too weak for " << becauseOf
        << ". Must be "
        << (expected < AccessControl::Private ? "at least '" : "'") << expected
        << "'";
};

/// General utility to quickly wrap arguments into a `utl::streammanip` object.
/// This should ideally be part of `utl`
static auto doFormat(auto... args) {
    return utl::streammanip([... args = std::move(args)](std::ostream& str) {
        ((str << args), ...);
    });
}

BadAccessControl::BadAccessControl(Scope const* scope, Entity const* entity,
                                   Reason reason):
    BadDecl(scope, dyncast<ast::Declaration const*>(entity->astNode()),
            IssueSeverity::Error),
    _reason(reason) {
    header([=](std::ostream& str) { str << "Invalid access control"; });
    switch (reason) {
    case TooWeakForParent:
        primary(sourceRange(), [=](std::ostream& str) {
            str << badAccCtrlMessage(entity->accessControl(),
                                     entity->parent()->accessControl(),
                                     doFormat("its parent ",
                                              sema::format(entity->parent())));
        });
        break;
    case TooWeakForType:
        primary(sourceRange(), [=](std::ostream& str) {
            str << badAccCtrlMessage(entity->accessControl(),
                                     getEntityType(*entity)->accessControl(),
                                     doFormat("its type ",
                                              sema::format(
                                                  getEntityType(*entity))));
        });
        break;
    }
}

static IssueSeverity toSeverity(BadReturnStmt::Reason reason) {
    switch (reason) {
#define SC_SEMA_BADRETURN_DEF(reason, severity, _)                             \
    case BadReturnStmt::reason:                                                \
        return IssueSeverity::severity;
#include "Sema/SemaIssues.def.h"
    }
    SC_UNREACHABLE();
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
#include "Sema/SemaIssues.def.h"
    }
}

static constexpr utl::streammanip formatRetType =
    [](std::ostream& str, ast::ReturnStatement const* stmt) {
    if (auto* expr = stmt->expression()) {
        str << format(expr->type());
    }
    else {
        str << tfmt::format(Bold | Magenta, "void");
    }
};

static SourceRange getRetSR(ast::ReturnStatement const* stmt) {
    if (auto* expr = stmt->expression()) {
        return expr->sourceRange();
    }
    return stmt->sourceRange();
}

BadReturnTypeDeduction::BadReturnTypeDeduction(
    Scope const* scope, ast::ReturnStatement const* stmt,
    ast::ReturnStatement const* confl):
    BadStmt(scope, stmt, IssueSeverity::Error), confl(confl) {
    header([=, this](std::ostream& str) {
        str << "Conflicting return type deduction in function '"
            << statement()->findAncestor<ast::FunctionDefinition>()->name()
            << "'";
    });
    highlight(Primary, getRetSR(statement()), [=, this](std::ostream& str) {
        str << "Here return type is deduced as " << formatRetType(statement());
    });
    highlight(Secondary, getRetSR(conflicting()), [=, this](std::ostream& str) {
        str << "Here return type was deduced as "
            << formatRetType(conflicting());
    });
}

StructDefCycle::StructDefCycle(Scope const* _scope,
                               std::vector<Entity const*> _cycle):
    BadDecl(_scope, nullptr, IssueSeverity::Error), _cycle(std::move(_cycle)) {
    header("Cyclic struct definition");
    hint("Declare data members as pointers to break strong dependencies");
    for (auto [index, entity]: cycle() | ranges::views::enumerate) {
        primary(getSourceRange(entity->astNode()),
                [=, this, entity = entity,
                 index = index + 1](std::ostream& str) {
            if (auto* var = dyncast<Variable const*>(entity)) {
                auto* type = cast<Type const*>(cycle()[index % cycle().size()]);
                str << index << ". Member " << var->name()
                    << " depends on definition of type " << sema::format(type);
            }
            else if (auto* type = dyncast<StructType const*>(entity)) {
                str << index << ". Struct " << sema::format(type)
                    << " defined here";
            }
        });
    }
}

static std::string typeDeductMutabilityHeader(ReferenceKind kind) {
    auto [a, b] = [&]() -> std::pair<char const*, char const*> {
        switch (kind) {
        case ReferenceKind::Reference:
            return { "immutable value", "mutable reference" };
        case ReferenceKind::Pointer:
            return { "pointer to immutable value", "pointer to mutable value" };
        case ReferenceKind::UniquePointer:
            return { "unique pointer to immutable value",
                     "unique pointer to mutable value" };
        }
    }();
    return utl::strcat("Cannot deduce ", a, " as ", b);
}

static std::string typeDeductNoPointerHeader(TypeDeductionQualifier const* qual,
                                             QualType type) {
    // clang-format off
    auto* value = SC_MATCH(*type) {
        [](ObjectType const&) { return "non-pointer value"; },
        [](RawPtrType const&) { return "pointer"; },
        [](UniquePtrType const&) { return "unique pointer"; },
    }; // clang-format on
    auto* as = [&] {
        switch (qual->refKind()) {
        case ReferenceKind::Reference:
            return "reference";
        case ReferenceKind::Pointer:
            return "pointer";
        case ReferenceKind::UniquePointer:
            return "unique pointer";
        }
    }();

    return utl::strcat("Cannot deduce ", value, " as ", as);
}

BadTypeDeduction::BadTypeDeduction(ast::Expression const* typeExpr,
                                   ast::Expression const* valueExpr,
                                   Reason reason):
    SemaIssue(nullptr, getSourceRange(typeExpr, valueExpr),
              IssueSeverity::Error),
    _reason(reason),
    _typeExpr(typeExpr) {
    switch (reason) {
    case Mutability: {
        auto* qual = cast<TypeDeductionQualifier const*>(typeExpr->entity());
        header(typeDeductMutabilityHeader(qual->refKind()));
        primary(getSourceRange(valueExpr), [=](std::ostream& str) {
            str << "Value of type " << sema::format(valueExpr->type());
        });
        secondary(getSourceRange(typeExpr),
                  [=](std::ostream& str) { str << "Deduction qualifier"; });
        break;
    }
    case MissingInitializer:
        header("Initializer required for type deduction");
        primary(getSourceRange(typeExpr), [=](std::ostream& str) {
            str << "Type deduction requested here";
        });
        break;
    case NoPointer: {
        auto* qual = cast<TypeDeductionQualifier const*>(typeExpr->entity());
        header(typeDeductNoPointerHeader(qual, valueExpr->type()));
        primary(getSourceRange(valueExpr), [=](std::ostream& str) {
            str << "Value of type " << sema::format(valueExpr->type());
        });
        break;
    }
    case InvalidContext: {
        primary(getSourceRange(typeExpr), [=](std::ostream& str) {
            str << "Type deduction is not allowed here";
        });
    }
    }
}

BadPassedType::BadPassedType(Scope const* scope, ast::Expression const* expr,
                             Reason reason):
    SemaIssue(scope, getSourceRange(expr), IssueSeverity::Error),
    _reason(reason) {
    switch (reason) {
    case Argument:
        header("Cannot pass parameter of incomplete type");
        primary(sourceRange(), [=](std::ostream& str) {
            str << "Parameter of incomplete type "
                << sema::format(cast<Type const*>(expr->entity()))
                << " declared here";
        });
        break;
    case Return:
        header("Cannot return value of incomplete type");
        primary(sourceRange(), [=](std::ostream& str) {
            str << "Incomplete return type "
                << sema::format(cast<Type const*>(expr->entity()))
                << " declared here";
        });
        break;
    case ReturnDeduced:
        header("Cannot return value of incomplete type");
        primary(sourceRange(), [=](std::ostream& str) {
            str << "Incomplete return type " << sema::formatType(expr)
                << " deduced here";
        });
        break;
    }
}

static IssueSeverity toSeverity(BadExpr::Reason reason) {
    switch (reason) {
#define SC_SEMA_BADEXPR_DEF(Type, Reason, Severity, Message)                   \
    case BadExpr::Reason:                                                      \
        return IssueSeverity::Severity;
#include "Sema/SemaIssues.def.h"
    }
    SC_UNREACHABLE();
}

BadExpr::BadExpr(Scope const* scope, ast::ASTNode const* expr, Reason reason):
    SemaIssue(scope, getSourceRange(expr), toSeverity(reason)),
    _reason(reason),
    _node(expr) {}

BadExpr::BadExpr(Scope const* scope, ast::ASTNode const* expr,
                 IssueSeverity severity):
    SemaIssue(scope, getSourceRange(expr), severity),
    _reason(BadExprNone),
    _node(expr) {}

ast::Expression const* BadExpr::expr() const {
    return dyncast<ast::Expression const*>(astNode());
}

void BadExpr::setExpr(ast::ASTNode const* expr) { _node = expr; }

void BadExpr::format(std::ostream& str) const {
    switch (reason()) {
#define SC_SEMA_BADEXPR_DEF(Type, Reason, Severity, Message)                   \
    case Reason: {                                                             \
        [[maybe_unused]] auto* expr =                                          \
            dyncast<ast::Type const*>(this->astNode());                        \
        str << Message;                                                        \
        break;                                                                 \
    }
#include "Sema/SemaIssues.def.h"
    }
}

BadSymRef::BadSymRef(Scope const* scope, ast::Expression const* expr,
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
    case Namespace:
        str << "Expected a namespace";
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

BadCleanup::BadCleanup(ast::ASTNode const* node, Object const* object):
    SemaIssue(object->parent(), getSourceRange(node), IssueSeverity::Error) {
    primary(sourceRange(), [=](std::ostream& str) {
        str << "Deletion of undeletable type " << sema::format(object->type())
            << " requested here";
    });
}

ORError::ORError(PrivateTag, ast::Expression const* expr,
                 std::span<Function const* const> os,
                 std::vector<ThinExpr> arguments,
                 std::vector<Function const*> matches,
                 std::unordered_map<Function const*, ORMatchError> matchErrors):
    SemaIssue(nullptr, getSourceRange(expr), IssueSeverity::Error),
    overloadSet(os | ranges::to<std::vector>),
    arguments(arguments),
    matches(matches),
    matchErrors(matchErrors) {
    SC_ASSERT(!overloadSet.empty(), "Cannot make call to empty overload set");
    header("Cannot resolve function call");
    auto name = overloadSet.front()->name();
    switch (reason()) {
    case NoMatch:
        primary(sourceRange(), [=](std::ostream& str) {
            str << "No matching function to call for " << name;
        });
        for (auto* function: overloadSet) {
            auto SR = getSourceRange(function->definition());
            secondary(SR, [=](std::ostream& str) {
                if (!SR.valid()) {
                    str << "Candidate function: ";
                    tfmt::FormatGuard guard(Reset, str);
                    str << sema::format(function) << "\n";
                }
                auto itr = matchErrors.find(function);
                if (itr == matchErrors.end()) {
                    str << "Cannot call this";
                    return;
                }
                ORMatchError err = itr->second;
                using enum ORMatchError::Reason;
                switch (err.reason) {
                case CountMismatch:
                    using namespace std::literals;
                    str << "Expected " << function->argumentCount()
                        << singularPlural(function->argumentCount(),
                                          " argument"sv, " arguments"sv)
                        << " but have " << arguments.size();
                    break;
                case NoArgumentConversion:
                    ThinExpr arg = arguments[err.argIndex];
                    auto* paramType = function->argumentType(err.argIndex);
                    str << "No conversion for " << formatOrdinal(err.argIndex)
                        << " argument from "
                        << (arg.type().isConst() ? "constant " : "mutable ")
                        << arg.valueCategory() << " of type "
                        << sema::format(arg.type()) << " to "
                        << sema::format(paramType);
                    break;
                }
            });
        }
        break;
    case Ambiguous:
        primary(sourceRange(), [=](std::ostream& str) {
            str << "Ambiguous function call to " << name;
        });
        for (auto* function: matches) {
            auto SR = getSourceRange(function->definition());
            secondary(SR, [=](std::ostream& str) {
                str << "Candidate function";
                if (!SR.valid()) {
                    str << ": ";
                    tfmt::FormatGuard guard(Reset, str);
                    str << sema::format(function);
                }
            });
        }
        break;
    }
}
