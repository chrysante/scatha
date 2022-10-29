#ifndef SCATHA_SEMA_SEMANTICISSUE_H_
#define SCATHA_SEMA_SEMANTICISSUE_H_

#include <iosfwd>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <variant>

#include <utl/utility.hpp>

#include "AST/AST.h"
#include "Basic/Basic.h"
#include "Common/Token.h"
#include "Issue/ProgramIssue.h"
#include "Sema/SymbolID.h"

namespace scatha::sema {

class Scope;

class SCATHA(API) IssueBase: public issue::ProgramIssueBase {
public:
    using issue::ProgramIssueBase::ProgramIssueBase;
};

/// MARK: Expression Issues
class SCATHA(API) BadExpression: public IssueBase {
public:
    explicit BadExpression(ast::Expression const& expr): IssueBase(expr.token()), _expr(&expr) {}

    ast::Expression const& expression() const { return *_expr; }

private:
    ast::Expression const* _expr;
};

class SCATHA(API) BadTypeConversion: public BadExpression {
public:
    explicit BadTypeConversion(ast::Expression const& expression, TypeID to):
        BadExpression(expression), _from(expression.typeID), _to(to) {}

    TypeID from() const { return _from; }
    TypeID to() const { return _to; }

private:
    TypeID _from;
    TypeID _to;
};

class SCATHA(API) BadOperandForUnaryExpression: public BadExpression {
public:
    explicit BadOperandForUnaryExpression(ast::Expression const& expression, TypeID operand):
        BadExpression(expression), _operand(operand) {}

    TypeID operand() const { return _operand; }

private:
    TypeID _operand;
};

class SCATHA(API) BadOperandsForBinaryExpression: public BadExpression {
public:
    explicit BadOperandsForBinaryExpression(ast::Expression const& expression, TypeID lhs, TypeID rhs):
        BadExpression(expression), _lhs(lhs), _rhs(rhs) {}

    TypeID lhs() const { return _lhs; }
    TypeID rhs() const { return _rhs; }

private:
    TypeID _lhs;
    TypeID _rhs;
};

class SCATHA(API) BadMemberAccess: public BadExpression {
public:
    using BadExpression::BadExpression;
};

class SCATHA(API) BadFunctionCall: public BadExpression {
public:
    enum class Reason { NoMatchingFunction, ObjectNotCallable, _count };

public:
    explicit BadFunctionCall(ast::Expression const& expression,
                             SymbolID overloadSetID,
                             utl::small_vector<TypeID> argTypeIDs,
                             Reason reason):
        BadExpression(expression), _reason(reason), _argTypeIDs(std::move(argTypeIDs)), _overloadSetID(overloadSetID) {}

    Reason reason() const { return _reason; }
    std::span<TypeID const> argumentTypeIDs() const { return _argTypeIDs; }
    SymbolID overloadSetID() const { return _overloadSetID; }

private:
    Reason _reason;
    utl::small_vector<TypeID> _argTypeIDs;
    SymbolID _overloadSetID;
};

SCATHA(API) std::ostream& operator<<(std::ostream&, BadFunctionCall::Reason);

class SCATHA(API) UseOfUndeclaredIdentifier: public BadExpression {
public:
    explicit UseOfUndeclaredIdentifier(ast::Expression const& expression, Scope const& inScope):
        BadExpression(expression), _scope(&inScope) {}

    Scope const& currentScope() const { return *_scope; }

private:
    Scope const* _scope;
};

class SCATHA(API) BadSymbolReference: public BadExpression {
public:
    explicit BadSymbolReference(ast::Expression const& expression,
                                ast::EntityCategory have,
                                ast::EntityCategory expected):
        BadExpression(expression), _have(have), _expected(expected) {}

    ast::EntityCategory have() const { return _have; }
    ast::EntityCategory expected() const { return _expected; }

private:
    ast::EntityCategory _have;
    ast::EntityCategory _expected;
};

/// MARK: Statement Issues
class SCATHA(API) InvalidStatement: public IssueBase {
public:
    enum class Reason { ExpectedDeclaration, InvalidDeclaration, InvalidScopeForStatement, _count };

public:
    explicit InvalidStatement(ast::Statement const* statement, Reason reason, Scope const& inScope):
        IssueBase(statement ? statement->token() : Token{}), _statement(statement), _reason(reason), _scope(&inScope) {}

    ast::Statement const& statement() const { return *_statement; }
    void setStatement(ast::Statement const& statement) {
        _statement = &statement;
        setToken(statement.token());
    }

    Reason reason() const { return _reason; }

    Scope const& currentScope() const { return *_scope; }

private:
    ast::Statement const* _statement;
    Reason _reason;
    Scope const* _scope;
};

SCATHA(API) std::ostream& operator<<(std::ostream&, InvalidStatement::Reason);

class SCATHA(API) InvalidDeclaration: public InvalidStatement {
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
    explicit InvalidDeclaration(ast::Statement const* statement,
                                Reason reason,
                                Scope const& currentScope,
                                SymbolCategory symbolCategory,
                                std::optional<SymbolCategory> existingSymbolCategory = std::nullopt):
        InvalidStatement(statement, InvalidStatement::Reason::InvalidDeclaration, currentScope),
        _reason(reason),
        _category(symbolCategory),
        _existingCategory(existingSymbolCategory.value_or(symbolCategory)) {}

    Reason reason() const { return _reason; }
    SymbolCategory symbolCategory() const { return _category; }

    /// Only valid when reason() == Redefinition
    SymbolCategory existingSymbolCategory() const { return _existingCategory; }

private:
    Reason _reason;
    SymbolCategory _category;
    SymbolCategory _existingCategory;
};

SCATHA(API)
std::ostream& operator<<(std::ostream&, InvalidDeclaration::Reason);

/// MARK: Cycles
class SCATHA(API) StrongReferenceCycle: public IssueBase {
public:
    struct Node {
        ast::AbstractSyntaxTree const* astNode;
        SymbolID symbolID;
    };

    explicit StrongReferenceCycle(utl::vector<Node> cycle):
        IssueBase(cycle.front().astNode->token()), _cycle(std::move(cycle)) {}

    void setStatement(ast::Statement const&) {}

    std::span<Node const> cycle() const { return _cycle; }

private:
    utl::vector<Node> _cycle;
};

/// MARK: Common class SemanticIssue
namespace internal {
using SemaIssueVariant = std::variant<BadTypeConversion,
                                      BadOperandForUnaryExpression,
                                      BadOperandsForBinaryExpression,
                                      BadMemberAccess,
                                      BadFunctionCall,
                                      UseOfUndeclaredIdentifier,
                                      BadSymbolReference,
                                      InvalidStatement,
                                      InvalidDeclaration,
                                      StrongReferenceCycle>;
}

class SCATHA(API) SemanticIssue: private internal::SemaIssueVariant {
private:
    template <typename T>
    static decltype(auto) visitImpl(auto&& f, auto&& v) {
        auto const vis = utl::visitor{ f, [](issue::internal::ProgramIssuePrivateBase&) -> T {
                                          if constexpr (!std::is_same_v<T, void>) {
                                              SC_DEBUGFAIL();
                                          }
                                      } };
        return std::visit(vis, v);
    }

public:
    using internal::SemaIssueVariant::SemaIssueVariant;

    template <typename T = void>
    decltype(auto) visit(auto&& f) {
        return visitImpl<T>(f, asBase());
    }

    template <typename T = void>
    decltype(auto) visit(auto&& f) const {
        return visitImpl<T>(f, asBase());
    }

    template <typename T>
    auto& get() {
        return std::get<T>(asBase());
    }

    template <typename T>
    auto const& get() const {
        return std::get<T>(asBase());
    }

    template <typename T>
    bool is() const {
        return std::holds_alternative<T>(asBase());
    }

    /// Weirdly enough this won't compile. We also don't really need the function though.
//  ast::Statement const& statement() const {
//      return visit<ast::Statement const&>([&](InvalidStatement const& e) -> auto& {
//          return e.statement();
//      });
//  }

    void setStatement(ast::Statement const& statement) {
        visit([&](InvalidStatement& e) { e.setStatement(statement); });
    }

    Token const& token() const {
        return visit([](issue::ProgramIssueBase const& base) -> auto& { return base.token(); });
    }

    void setToken(Token token) {
        visit([&](IssueBase& e) { e.setToken(std::move(token)); });
    }

private:
    internal::SemaIssueVariant& asBase() { return *this; }
    internal::SemaIssueVariant const& asBase() const { return *this; }
};

} // namespace scatha::sema

#endif // SCATHA_SEMA_SEMANTICISSUE_H_
