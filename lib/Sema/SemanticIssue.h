#ifndef SCATHA_SEMA_SEMAISSUE2_H_
#define SCATHA_SEMA_SEMAISSUE2_H_

#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <variant>

#include <utl/utility.hpp>
#include <utl/vector.hpp>

#include <scatha/AST/Fwd.h>
#include <scatha/Common/Base.h>
#include <scatha/Issue/Issue.h>
#include <scatha/Parser/Token.h>
#include <scatha/Sema/SymbolID.h>

namespace scatha::sema {

class Scope;

class SCATHA_API SemanticIssue: public Issue {
public:
    using Issue::Issue;

    virtual SemanticIssue* setStatement(ast::Statement const& statement) {
        return this;
    }
};

/// General expression issue
class SCATHA_API BadExpression: public SemanticIssue {
public:
    explicit BadExpression(ast::Expression const& expr, IssueSeverity severity);

    ast::Expression const& expression() const { return *_expr; }

private:
    std::string message() const override { return "Bad expression"; }

    ast::Expression const* _expr;
};

/// Invalid type conversion issue
class SCATHA_API BadTypeConversion: public BadExpression {
public:
    explicit BadTypeConversion(ast::Expression const& expression, TypeID to);

    TypeID from() const { return _from; }
    TypeID to() const { return _to; }

private:
    std::string message() const override { return "Invalid type conversion"; }

    TypeID _from;
    TypeID _to;
};

class SCATHA_API BadOperandForUnaryExpression: public BadExpression {
public:
    explicit BadOperandForUnaryExpression(ast::Expression const& expression,
                                          TypeID operand);

    TypeID operand() const { return _operand; }

private:
    TypeID _operand;
};

class SCATHA_API BadOperandsForBinaryExpression: public BadExpression {
public:
    explicit BadOperandsForBinaryExpression(ast::Expression const& expression,
                                            TypeID lhs,
                                            TypeID rhs):
        BadExpression(expression, IssueSeverity::Error), _lhs(lhs), _rhs(rhs) {}

    TypeID lhs() const { return _lhs; }
    TypeID rhs() const { return _rhs; }

private:
    std::string message() const override {
        return "Invalid operands for binary expression";
    }

    TypeID _lhs;
    TypeID _rhs;
};

class SCATHA_API BadMemberAccess: public BadExpression {
public:
    explicit BadMemberAccess(ast::Expression const& expr):
        BadExpression(expr, IssueSeverity::Error) {}
};

class SCATHA_API BadFunctionCall: public BadExpression {
public:
    enum class Reason { NoMatchingFunction, ObjectNotCallable, _count };

public:
    explicit BadFunctionCall(ast::Expression const& expression,
                             SymbolID overloadSetID,
                             utl::small_vector<TypeID> argTypeIDs,
                             Reason reason):
        BadExpression(expression, IssueSeverity::Error),
        _reason(reason),
        _argTypeIDs(std::move(argTypeIDs)),
        _overloadSetID(overloadSetID) {}

    Reason reason() const { return _reason; }
    std::span<TypeID const> argumentTypeIDs() const { return _argTypeIDs; }
    SymbolID overloadSetID() const { return _overloadSetID; }

private:
    std::string message() const override {
        return "No matching function to call";
    }

    Reason _reason;
    utl::small_vector<TypeID> _argTypeIDs;
    SymbolID _overloadSetID;
};

SCATHA_API std::ostream& operator<<(std::ostream&, BadFunctionCall::Reason);

class SCATHA_API UseOfUndeclaredIdentifier: public BadExpression {
public:
    explicit UseOfUndeclaredIdentifier(ast::Expression const& expression,
                                       Scope const& inScope):
        BadExpression(expression, IssueSeverity::Error), _scope(&inScope) {}

    Scope const& currentScope() const { return *_scope; }

private:
    Scope const* _scope;
};

class SCATHA_API BadSymbolReference: public BadExpression {
public:
    explicit BadSymbolReference(ast::Expression const& expression,
                                ast::EntityCategory have,
                                ast::EntityCategory expected):
        BadExpression(expression, IssueSeverity::Error),
        _have(have),
        _expected(expected) {}

    ast::EntityCategory have() const { return _have; }
    ast::EntityCategory expected() const { return _expected; }

private:
    std::string message() const override { return "Invalid symbol category"; }

    ast::EntityCategory _have;
    ast::EntityCategory _expected;
};

/// MARK: Statement Issues
class SCATHA_API InvalidStatement: public SemanticIssue {
public:
    enum class Reason {
        ExpectedDeclaration,
        InvalidDeclaration,
        InvalidScopeForStatement,
        _count
    };

public:
    explicit InvalidStatement(ast::Statement const* statement,
                              Reason reason,
                              Scope const& inScope);

    ast::Statement const& statement() const { return *_statement; }

    InvalidStatement* setStatement(ast::Statement const& statement) override;

    Reason reason() const { return _reason; }

    Scope const& currentScope() const { return *_scope; }

private:
    std::string message() const override { return "Invalid statement"; }

    ast::Statement const* _statement;
    Reason _reason;
    Scope const* _scope;
};

SCATHA_API std::ostream& operator<<(std::ostream&, InvalidStatement::Reason);

class SCATHA_API InvalidDeclaration: public InvalidStatement {
public:
    enum class Reason {
        InvalidInCurrentScope,
        Redefinition,
        CantOverloadOnReturnType,
        CantInferType,
        ReservedIdentifier,
        _count
    };

public:
    explicit InvalidDeclaration(
        ast::Statement const* statement,
        Reason reason,
        Scope const& currentScope,
        SymbolCategory symbolCategory,
        std::optional<SymbolCategory> existingSymbolCategory = std::nullopt):
        InvalidStatement(statement,
                         InvalidStatement::Reason::InvalidDeclaration,
                         currentScope),
        _reason(reason),
        _category(symbolCategory),
        _existingCategory(existingSymbolCategory.value_or(symbolCategory)) {}

    Reason reason() const { return _reason; }
    SymbolCategory symbolCategory() const { return _category; }

    /// Only valid when reason() == Redefinition
    SymbolCategory existingSymbolCategory() const { return _existingCategory; }

private:
    std::string message() const override { return "Invalid declaration"; }

    Reason _reason;
    SymbolCategory _category;
    SymbolCategory _existingCategory;
};

SCATHA_API std::ostream& operator<<(std::ostream&, InvalidDeclaration::Reason);

/// MARK: Cycles
class SCATHA_API StrongReferenceCycle: public SemanticIssue {
public:
    struct Node {
        ast::AbstractSyntaxTree const* astNode;
        SymbolID symbolID;
    };

    explicit StrongReferenceCycle(utl::vector<Node> cycle);

    std::span<Node const> cycle() const { return _cycle; }

private:
    std::string message() const override { return "Strong reference cycle"; }

    utl::vector<Node> _cycle;
};

} // namespace scatha::sema

#endif // SCATHA_SEMA_SEMAISSUE2_H_
