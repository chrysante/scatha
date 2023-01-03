#ifndef SCATHA_AST_COMMON_H_
#define SCATHA_AST_COMMON_H_

#include <concepts>
#include <iosfwd>
#include <string_view>

#include "Basic/Basic.h"
#include "Common/Dyncast.h"

namespace scatha::ast {

///
/// **Forward Declaration of all AST nodes**
///

class AbstractSyntaxTree;
class TranslationUnit;
class CompoundStatement;
class Declaration;
class FunctionDefinition;
class StructDefinition;
class VariableDeclaration;
class ParameterDeclaration;
class Statement;
class ExpressionStatement;
class EmptyStatement;
class ReturnStatement;
class IfStatement;
class WhileStatement;
class DoWhileStatement;
class Expression;
class Identifier;
class IntegerLiteral;
class BooleanLiteral;
class FloatingPointLiteral;
class StringLiteral;
class UnaryPrefixExpression;
class BinaryExpression;
class MemberAccess;
class Conditional;
class FunctionCall;
class Subscript;

/// List of all concrete AST node types.
enum class NodeType {
    AbstractSyntaxTree,
    TranslationUnit,
    CompoundStatement,
    Declaration,
    FunctionDefinition,
    StructDefinition,
    VariableDeclaration,
    ParameterDeclaration,
    Statement,
    ExpressionStatement,
    EmptyStatement,
    ReturnStatement,
    IfStatement,
    WhileStatement,
    DoWhileStatement,
    Expression,
    Identifier,
    IntegerLiteral,
    BooleanLiteral,
    FloatingPointLiteral,
    StringLiteral,
    UnaryPrefixExpression,
    BinaryExpression,
    MemberAccess,
    Conditional,
    FunctionCall,
    Subscript,

    _count
};

bool isDeclaration(NodeType);

SCATHA(API) std::string_view toString(NodeType);

SCATHA(API) std::ostream& operator<<(std::ostream&, NodeType);

} // namespace scatha::ast

#define SC_AST_ENABLE_DYNCAST(type) SC_DYNCAST_MAP(::scatha::ast::type, ::scatha::ast::NodeType::type)

SC_AST_ENABLE_DYNCAST(AbstractSyntaxTree);
SC_AST_ENABLE_DYNCAST(TranslationUnit);
SC_AST_ENABLE_DYNCAST(CompoundStatement);
SC_AST_ENABLE_DYNCAST(Declaration);
SC_AST_ENABLE_DYNCAST(FunctionDefinition);
SC_AST_ENABLE_DYNCAST(StructDefinition);
SC_AST_ENABLE_DYNCAST(VariableDeclaration);
SC_AST_ENABLE_DYNCAST(ParameterDeclaration);
SC_AST_ENABLE_DYNCAST(Statement);
SC_AST_ENABLE_DYNCAST(ExpressionStatement);
SC_AST_ENABLE_DYNCAST(EmptyStatement);
SC_AST_ENABLE_DYNCAST(ReturnStatement);
SC_AST_ENABLE_DYNCAST(IfStatement);
SC_AST_ENABLE_DYNCAST(WhileStatement);
SC_AST_ENABLE_DYNCAST(DoWhileStatement);
SC_AST_ENABLE_DYNCAST(Expression);
SC_AST_ENABLE_DYNCAST(Identifier);
SC_AST_ENABLE_DYNCAST(IntegerLiteral);
SC_AST_ENABLE_DYNCAST(BooleanLiteral);
SC_AST_ENABLE_DYNCAST(FloatingPointLiteral);
SC_AST_ENABLE_DYNCAST(StringLiteral);
SC_AST_ENABLE_DYNCAST(UnaryPrefixExpression);
SC_AST_ENABLE_DYNCAST(BinaryExpression);
SC_AST_ENABLE_DYNCAST(MemberAccess);
SC_AST_ENABLE_DYNCAST(Conditional);
SC_AST_ENABLE_DYNCAST(FunctionCall);
SC_AST_ENABLE_DYNCAST(Subscript);

#undef SC_AST_ENABLE_DYNCAST

namespace scatha::ast {

/// List of all unary operators in prefix notation
enum class UnaryPrefixOperator {
    Promotion,
    Negation,
    BitwiseNot,
    LogicalNot,

    _count
};

SCATHA(API) std::string_view toString(UnaryPrefixOperator);

SCATHA(API) std::ostream& operator<<(std::ostream&, UnaryPrefixOperator);

/// List of all binary operators in infix notation
enum class BinaryOperator {
    Multiplication,
    Division,
    Remainder,
    Addition,
    Subtraction,
    LeftShift,
    RightShift,
    Less,
    LessEq,
    Greater,
    GreaterEq,
    Equals,
    NotEquals,
    BitwiseAnd,
    BitwiseXOr,
    BitwiseOr,
    LogicalAnd,
    LogicalOr,
    Assignment,
    AddAssignment,
    SubAssignment,
    MulAssignment,
    DivAssignment,
    RemAssignment,
    LSAssignment,
    RSAssignment,
    AndAssignment,
    OrAssignment,
    XOrAssignment,
    Comma,

    _count
};

SCATHA(API) std::string_view toString(BinaryOperator);

SCATHA(API) std::ostream& operator<<(std::ostream&, BinaryOperator);

BinaryOperator toNonAssignment(BinaryOperator);

SCATHA(API) bool isArithmetic(BinaryOperator);
SCATHA(API) bool isLogical(BinaryOperator);
SCATHA(API) bool isComparison(BinaryOperator);
SCATHA(API) bool isAssignment(BinaryOperator);

enum class EntityCategory { Value, Type, _count };

SCATHA(API) std::ostream& operator<<(std::ostream&, EntityCategory);

/// MARK: DefaultVisitor

namespace internal {

template <typename Derived, typename Base>
concept WeakDerivedFrom = std::derived_from<std::remove_cvref_t<Derived>, Base>;

template <typename T, typename U>
concept WeakSameAs = std::same_as<std::remove_cvref_t<T>, std::remove_cvref_t<U>>;

} // namespace internal

template <typename F>
struct DefaultVisitor {
    F callback;

    explicit DefaultVisitor(F callback): callback(callback) {}

    void operator()(AbstractSyntaxTree const&) const {}
    void operator()(internal::WeakSameAs<TranslationUnit> auto&& tu) const {
        for (auto&& decl: tu.declarations) {
            std::invoke(callback, *decl);
        }
    }
    void operator()(internal::WeakSameAs<CompoundStatement> auto&& b) const {
        for (auto& s: b.statements) {
            std::invoke(callback, *s);
        }
    }
    void operator()(internal::WeakSameAs<FunctionDefinition> auto&& fn) const {
        for (auto& param: fn.parameters) {
            std::invoke(callback, *param);
        }
        std::invoke(callback, *fn.body);
    }
    void operator()(internal::WeakSameAs<StructDefinition> auto&& s) const { std::invoke(callback, *s.body); }
    void operator()(internal::WeakSameAs<VariableDeclaration> auto&& s) const {
        if (s.typeExpr) {
            std::invoke(callback, *s.typeExpr);
        }
        if (s.initExpression) {
            std::invoke(callback, *s.initExpression);
        }
    }
    void operator()(internal::WeakSameAs<ExpressionStatement> auto&& s) const {
        SC_ASSERT(s.expression != nullptr, "");
        std::invoke(callback, *s.expression);
    }
    void operator()(internal::WeakSameAs<ReturnStatement> auto&& s) const {
        if (s.expression) {
            std::invoke(callback, *s.expression);
        }
    }
    void operator()(internal::WeakSameAs<IfStatement> auto&& s) const {
        std::invoke(callback, *s.condition);
        std::invoke(callback, *s.ifBlock);
        if (s.elseBlock != nullptr) {
            std::invoke(callback, *s.elseBlock);
        }
    }
    void operator()(internal::WeakSameAs<WhileStatement> auto&& s) const {
        std::invoke(callback, *s.condition);
        std::invoke(callback, *s.block);
    }
    void operator()(internal::WeakSameAs<IntegerLiteral> auto&&) const {}
    void operator()(internal::WeakSameAs<BooleanLiteral> auto&&) const {}
    void operator()(internal::WeakSameAs<FloatingPointLiteral> auto&&) const {}
    void operator()(internal::WeakSameAs<StringLiteral> auto&&) const {}
    void operator()(internal::WeakSameAs<UnaryPrefixExpression> auto&& e) const { std::invoke(callback, *e.operand); }
    void operator()(internal::WeakSameAs<BinaryExpression> auto&& e) const {
        std::invoke(callback, *e.lhs);
        std::invoke(callback, *e.rhs);
    }
    void operator()(internal::WeakSameAs<MemberAccess> auto&& m) const { callback(*m.object); }
    void operator()(internal::WeakSameAs<Conditional> auto&& c) const {
        std::invoke(callback, *c.condition);
        std::invoke(callback, *c.ifExpr);
        std::invoke(callback, *c.elseExpr);
    }
    void operator()(internal::WeakSameAs<FunctionCall> auto&& f) const {
        std::invoke(callback, *f.object);
        for (auto& arg: f.arguments) {
            std::invoke(callback, *arg);
        }
    }
    void operator()(internal::WeakSameAs<Subscript> auto&& s) const {
        std::invoke(callback, *s.object);
        for (auto& arg: s.arguments) {
            std::invoke(callback, *arg);
        }
    }
};

} // namespace scatha::ast

#endif // SCATHA_AST_COMMON_H_
