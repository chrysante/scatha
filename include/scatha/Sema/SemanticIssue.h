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
#include <scatha/Sema/QualType.h>

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
    explicit BadExpression(ast::Expression const& expr,
                           IssueSeverity severity = IssueSeverity::Error);

    ast::Expression const& expression() const { return *_expr; }

private:
    void format(std::ostream&) const override;

    ast::Expression const* _expr;
};

class SCATHA_API AssignmentToConst: public BadExpression {
public:
    explicit AssignmentToConst(ast::Expression const& expression);

private:
    void format(std::ostream&) const override;
};

/// Invalid type conversion issue
class SCATHA_API BadTypeConversion: public BadExpression {
public:
    explicit BadTypeConversion(ast::Expression const& expression, QualType to);

    QualType from() const { return _from; }
    QualType to() const { return _to; }

private:
    void format(std::ostream&) const override;

    QualType _from;
    QualType _to;
};

class SCATHA_API BadOperandForUnaryExpression: public BadExpression {
public:
    explicit BadOperandForUnaryExpression(ast::Expression const& expression,
                                          QualType operandType);

    QualType operandType() const { return _opType; }

private:
    void format(std::ostream&) const override;

    QualType _opType;
};

class SCATHA_API BadOperandsForBinaryExpression: public BadExpression {
public:
    explicit BadOperandsForBinaryExpression(ast::Expression const& expression,
                                            QualType lhs,
                                            QualType rhs):
        BadExpression(expression, IssueSeverity::Error), _lhs(lhs), _rhs(rhs) {}

    QualType lhs() const { return _lhs; }
    QualType rhs() const { return _rhs; }

private:
    void format(std::ostream&) const override;

    QualType _lhs;
    QualType _rhs;
};

class SCATHA_API BadMemberAccess: public BadExpression {
public:
    explicit BadMemberAccess(ast::Expression const& expr):
        BadExpression(expr, IssueSeverity::Error) {}

    /// TODO: Add format() override
};

class SCATHA_API BadFunctionCall: public BadExpression {
public:
    enum class Reason { NoMatchingFunction, ObjectNotCallable, _count };

public:
    explicit BadFunctionCall(ast::Expression const& expression,
                             OverloadSet const* overloadSet,
                             utl::small_vector<QualType> argTypes,
                             Reason reason):
        BadExpression(expression, IssueSeverity::Error),
        _reason(reason),
        _argTypes(std::move(argTypes)),
        _overloadSet(overloadSet) {}

    Reason reason() const { return _reason; }

    std::span<QualType const> argumentTypes() const { return _argTypes; }

    OverloadSet const* overloadSet() const { return _overloadSet; }

private:
    void format(std::ostream&) const override;

    Reason _reason;
    utl::small_vector<QualType> _argTypes;
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
    void format(std::ostream&) const override;

    Scope const* _scope;
};

class SCATHA_API BadSymbolReference: public BadExpression {
public:
    explicit BadSymbolReference(ast::Expression const& expression,
                                EntityCategory expected);

    EntityCategory have() const { return _have; }
    EntityCategory expected() const { return _expected; }

private:
    void format(std::ostream&) const override;

    EntityCategory _have;
    EntityCategory _expected;
};

/// MARK: Statement Issues
class SCATHA_API InvalidStatement: public SemanticIssue {
public:
    enum class Reason {
        ExpectedDeclaration,
        InvalidDeclaration,
        InvalidJump,
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
    void format(std::ostream&) const override;

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
        InvalidSpecialMemberFunction,
        _count
    };

    explicit InvalidDeclaration(ast::Statement const* statement,
                                Reason reason,
                                Scope const& currentScope):
        InvalidStatement(statement,
                         InvalidStatement::Reason::InvalidDeclaration,
                         currentScope),
        _reason(reason) {}

    Reason reason() const { return _reason; }

private:
    void format(std::ostream&) const override;

    Reason _reason;
};

SCATHA_API std::ostream& operator<<(std::ostream&, InvalidDeclaration::Reason);

/// MARK: Cycles
class SCATHA_API StrongReferenceCycle: public SemanticIssue {
public:
    struct Node {
        ast::AbstractSyntaxTree const* astNode = nullptr;
        sema::Entity const* entity = nullptr;
    };

    explicit StrongReferenceCycle(utl::vector<Node> cycle);

    std::span<Node const> cycle() const { return _cycle; }

private:
    void format(std::ostream&) const override;

    utl::vector<Node> _cycle;
};

class SCATHA_API InvalidListExpr: public BadExpression {
public:
    enum Reason {
        InvalidElemCountForArrayType,
        InvalidArrayCount,
        NoCommonType,
        _count
    };

    explicit InvalidListExpr(ast::ListExpression const& expr, Reason reason);

    Reason reason() const { return _reason; }

private:
    void format(std::ostream&) const override;

    Reason _reason;
};

SCATHA_API std::ostream& operator<<(std::ostream& ostream,
                                    InvalidListExpr::Reason reason);

class SCATHA_API InvalidNameLookup: public BadExpression {
public:
    explicit InvalidNameLookup(ast::Expression const& expr):
        BadExpression(expr, IssueSeverity::Error) {}

private:
    void format(std::ostream&) const override;
};

} // namespace scatha::sema

#endif // SCATHA_SEMA_SEMAISSUE_H_
