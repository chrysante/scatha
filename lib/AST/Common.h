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

// clang-format off
SC_DYNCAST_MAP(scatha::ast::AbstractSyntaxTree,    scatha::ast::NodeType::AbstractSyntaxTree);
SC_DYNCAST_MAP(scatha::ast::TranslationUnit,       scatha::ast::NodeType::TranslationUnit);
SC_DYNCAST_MAP(scatha::ast::CompoundStatement,     scatha::ast::NodeType::CompoundStatement);
SC_DYNCAST_MAP(scatha::ast::Declaration,           scatha::ast::NodeType::Declaration);
SC_DYNCAST_MAP(scatha::ast::FunctionDefinition,    scatha::ast::NodeType::FunctionDefinition);
SC_DYNCAST_MAP(scatha::ast::StructDefinition,      scatha::ast::NodeType::StructDefinition);
SC_DYNCAST_MAP(scatha::ast::VariableDeclaration,   scatha::ast::NodeType::VariableDeclaration);
SC_DYNCAST_MAP(scatha::ast::ParameterDeclaration,  scatha::ast::NodeType::ParameterDeclaration);
SC_DYNCAST_MAP(scatha::ast::Statement,             scatha::ast::NodeType::Statement);
SC_DYNCAST_MAP(scatha::ast::ExpressionStatement,   scatha::ast::NodeType::ExpressionStatement);
SC_DYNCAST_MAP(scatha::ast::EmptyStatement,        scatha::ast::NodeType::EmptyStatement);
SC_DYNCAST_MAP(scatha::ast::ReturnStatement,       scatha::ast::NodeType::ReturnStatement);
SC_DYNCAST_MAP(scatha::ast::IfStatement,           scatha::ast::NodeType::IfStatement);
SC_DYNCAST_MAP(scatha::ast::WhileStatement,        scatha::ast::NodeType::WhileStatement);
SC_DYNCAST_MAP(scatha::ast::DoWhileStatement,      scatha::ast::NodeType::DoWhileStatement);
SC_DYNCAST_MAP(scatha::ast::Expression,            scatha::ast::NodeType::Expression);
SC_DYNCAST_MAP(scatha::ast::Identifier,            scatha::ast::NodeType::Identifier);
SC_DYNCAST_MAP(scatha::ast::IntegerLiteral,        scatha::ast::NodeType::IntegerLiteral);
SC_DYNCAST_MAP(scatha::ast::BooleanLiteral,        scatha::ast::NodeType::BooleanLiteral);
SC_DYNCAST_MAP(scatha::ast::FloatingPointLiteral,  scatha::ast::NodeType::FloatingPointLiteral);
SC_DYNCAST_MAP(scatha::ast::StringLiteral,         scatha::ast::NodeType::StringLiteral);
SC_DYNCAST_MAP(scatha::ast::UnaryPrefixExpression, scatha::ast::NodeType::UnaryPrefixExpression);
SC_DYNCAST_MAP(scatha::ast::BinaryExpression,      scatha::ast::NodeType::BinaryExpression);
SC_DYNCAST_MAP(scatha::ast::MemberAccess,          scatha::ast::NodeType::MemberAccess);
SC_DYNCAST_MAP(scatha::ast::Conditional,           scatha::ast::NodeType::Conditional);
SC_DYNCAST_MAP(scatha::ast::FunctionCall,          scatha::ast::NodeType::FunctionCall);
SC_DYNCAST_MAP(scatha::ast::Subscript,             scatha::ast::NodeType::Subscript);
// clang-format on

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
