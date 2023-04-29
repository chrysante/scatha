#ifndef SCATHA_SEMA_SEMAISSUE_H_
#define SCATHA_SEMA_SEMAISSUE_H_

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
#include <scatha/Sema/Fwd.h>

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
    explicit BadTypeConversion(ast::Expression const& expression,
                               QualType const* to);

    QualType const* from() const { return _from; }
    QualType const* to() const { return _to; }

private:
    std::string message() const override { return "Invalid type conversion"; }

    QualType const* _from;
    QualType const* _to;
};

class SCATHA_API BadOperandForUnaryExpression: public BadExpression {
public:
    explicit BadOperandForUnaryExpression(ast::Expression const& expression,
                                          QualType const* operandType);

    QualType const* operandType() const { return _opType; }

private:
    QualType const* _opType;
};

class SCATHA_API BadOperandsForBinaryExpression: public BadExpression {
public:
    explicit BadOperandsForBinaryExpression(ast::Expression const& expression,
                                            QualType const* lhs,
                                            QualType const* rhs):
        BadExpression(expression, IssueSeverity::Error), _lhs(lhs), _rhs(rhs) {}

    QualType const* lhs() const { return _lhs; }
    QualType const* rhs() const { return _rhs; }

private:
    std::string message() const override {
        return "Invalid operands for binary expression";
    }

    QualType const* _lhs;
    QualType const* _rhs;
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
                             OverloadSet const* overloadSet,
                             utl::small_vector<QualType const*> argTypes,
                             Reason reason):
        BadExpression(expression, IssueSeverity::Error),
        _reason(reason),
        _argTypes(std::move(argTypes)),
        _overloadSet(overloadSet) {}

    Reason reason() const { return _reason; }

    std::span<QualType const* const> argumentTypes() const { return _argTypes; }

    OverloadSet const* overloadSet() const { return _overloadSet; }

private:
    std::string message() const override {
        return "No matching function to call";
    }

    Reason _reason;
    utl::small_vector<QualType const*> _argTypes;
    OverloadSet const* _overloadSet;
};

SCATHA_API std::ostream& operator<<(std::ostream&, BadFunctionCall::Reason);

class SCATHA_API UseOfUndeclaredIdentifier: public BadExpression {
public:
    explicit UseOfUndeclaredIdentifier(ast::Expression const& expression,
                                       Scope const& inScope):
        BadExpression(expression, IssueSeverity::Error), _scope(&inScope) {}

    Scope const& currentScope() const { return *_scope; }

private:
    std::string message() const override;

    Scope const* _scope;
};

class SCATHA_API BadSymbolReference: public BadExpression {
public:
    explicit BadSymbolReference(ast::Expression const& expression,
                                EntityCategory expected);

    EntityCategory have() const { return _have; }
    EntityCategory expected() const { return _expected; }

private:
    std::string message() const override { return "Invalid symbol category"; }

    EntityCategory _have;
    EntityCategory _expected;
};

/// MARK: Statement Issues
class SCATHA_API InvalidStatement: public SemanticIssue {
public:
    enum class Reason {
        ExpectedDeclaration,
        InvalidDeclaration,
        InvalidScopeForStatement,
        NonVoidFunctionMustReturnAValue,
        VoidFunctionMustNotReturnAValue,
        _count
    };

public:
    explicit InvalidStatement(ast::Statement const* statement,
                              Reason reason,
                              Scope const& inScope);

    ast::Statement const& statement() const { return *_statement; }

    InvalidStatement* setStatement(ast::Statement const& statement) override;

    Reason reason() const { return _reason; }

    Scope const* currentScope() const { return _scope; }

private:
    std::string message() const override;

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
        ExpectedReferenceInitializer,
        ReservedIdentifier,
        ThisParameter,
        _count
    };

public:
    explicit InvalidDeclaration(ast::Statement const* statement,
                                Reason reason,
                                Scope const& currentScope):
        InvalidStatement(statement,
                         InvalidStatement::Reason::InvalidDeclaration,
                         currentScope),
        _reason(reason) {}

    Reason reason() const { return _reason; }

private:
    std::string message() const override;

    Reason _reason;
};

SCATHA_API std::ostream& operator<<(std::ostream&, InvalidDeclaration::Reason);

/// MARK: Cycles
class SCATHA_API StrongReferenceCycle: public SemanticIssue {
public:
    struct Node {
        ast::AbstractSyntaxTree const* astNode = nullptr;
        sema::Entity const* entity             = nullptr;
    };

    explicit StrongReferenceCycle(utl::vector<Node> cycle);

    std::span<Node const> cycle() const { return _cycle; }

private:
    std::string message() const override { return "Strong reference cycle"; }

    utl::vector<Node> _cycle;
};

} // namespace scatha::sema

#endif // SCATHA_SEMA_SEMAISSUE_H_
