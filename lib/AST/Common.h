#ifndef SCATHA_AST_COMMON_H_
#define SCATHA_AST_COMMON_H_

#include <concepts>
#include <iosfwd>
#include <string_view>

#include "Basic/Basic.h"

namespace scatha::ast {

///
/// Forward Declaration of all AST nodes
///

struct AbstractSyntaxTree;
struct TranslationUnit;
struct CompoundStatement;
struct FunctionDefinition;
struct StructDefinition;
struct VariableDeclaration;
struct ParameterDeclaration;
struct ExpressionStatement;
struct EmptyStatement;
struct ReturnStatement;
struct IfStatement;
struct WhileStatement;
struct Identifier;
struct IntegerLiteral;
struct BooleanLiteral;
struct FloatingPointLiteral;
struct StringLiteral;
struct UnaryPrefixExpression;
struct BinaryExpression;
struct MemberAccess;
struct Conditional;
struct FunctionCall;
struct Subscript;

/// List of all concrete AST node types
enum class NodeType {
    TranslationUnit,
    CompoundStatement,
    FunctionDefinition,
    StructDefinition,
    VariableDeclaration,
    ParameterDeclaration,
    ExpressionStatement,
    EmptyStatement,
    ReturnStatement,
    IfStatement,
    WhileStatement,
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

namespace internal {
template <typename>
struct ToEnumNodeTypeImpl;
template <>
struct ToEnumNodeTypeImpl<TranslationUnit>: std::integral_constant<NodeType, NodeType::TranslationUnit> {};
template <>
struct ToEnumNodeTypeImpl<CompoundStatement>: std::integral_constant<NodeType, NodeType::CompoundStatement> {};
template <>
struct ToEnumNodeTypeImpl<FunctionDefinition>: std::integral_constant<NodeType, NodeType::FunctionDefinition> {};
template <>
struct ToEnumNodeTypeImpl<StructDefinition>: std::integral_constant<NodeType, NodeType::StructDefinition> {};
template <>
struct ToEnumNodeTypeImpl<VariableDeclaration>: std::integral_constant<NodeType, NodeType::VariableDeclaration> {};
template <>
struct ToEnumNodeTypeImpl<ParameterDeclaration>: std::integral_constant<NodeType, NodeType::ParameterDeclaration> {};
template <>
struct ToEnumNodeTypeImpl<ExpressionStatement>: std::integral_constant<NodeType, NodeType::ExpressionStatement> {};
template <>
struct ToEnumNodeTypeImpl<EmptyStatement>: std::integral_constant<NodeType, NodeType::EmptyStatement> {};
template <>
struct ToEnumNodeTypeImpl<ReturnStatement>: std::integral_constant<NodeType, NodeType::ReturnStatement> {};
template <>
struct ToEnumNodeTypeImpl<IfStatement>: std::integral_constant<NodeType, NodeType::IfStatement> {};
template <>
struct ToEnumNodeTypeImpl<WhileStatement>: std::integral_constant<NodeType, NodeType::WhileStatement> {};
template <>
struct ToEnumNodeTypeImpl<Identifier>: std::integral_constant<NodeType, NodeType::Identifier> {};
template <>
struct ToEnumNodeTypeImpl<IntegerLiteral>: std::integral_constant<NodeType, NodeType::IntegerLiteral> {};
template <>
struct ToEnumNodeTypeImpl<BooleanLiteral>: std::integral_constant<NodeType, NodeType::BooleanLiteral> {};
template <>
struct ToEnumNodeTypeImpl<FloatingPointLiteral>: std::integral_constant<NodeType, NodeType::FloatingPointLiteral> {};
template <>
struct ToEnumNodeTypeImpl<StringLiteral>: std::integral_constant<NodeType, NodeType::StringLiteral> {};
template <>
struct ToEnumNodeTypeImpl<UnaryPrefixExpression>: std::integral_constant<NodeType, NodeType::UnaryPrefixExpression> {};
template <>
struct ToEnumNodeTypeImpl<BinaryExpression>: std::integral_constant<NodeType, NodeType::BinaryExpression> {};
template <>
struct ToEnumNodeTypeImpl<MemberAccess>: std::integral_constant<NodeType, NodeType::MemberAccess> {};
template <>
struct ToEnumNodeTypeImpl<Conditional>: std::integral_constant<NodeType, NodeType::Conditional> {};
template <>
struct ToEnumNodeTypeImpl<FunctionCall>: std::integral_constant<NodeType, NodeType::FunctionCall> {};
template <>
struct ToEnumNodeTypeImpl<Subscript>: std::integral_constant<NodeType, NodeType::Subscript> {};

template <NodeType>
struct ToNodeTypeImpl;
template <>
struct ToNodeTypeImpl<NodeType::TranslationUnit> {
    using type = TranslationUnit;
};
template <>
struct ToNodeTypeImpl<NodeType::CompoundStatement> {
    using type = CompoundStatement;
};
template <>
struct ToNodeTypeImpl<NodeType::FunctionDefinition> {
    using type = FunctionDefinition;
};
template <>
struct ToNodeTypeImpl<NodeType::StructDefinition> {
    using type = StructDefinition;
};
template <>
struct ToNodeTypeImpl<NodeType::VariableDeclaration> {
    using type = VariableDeclaration;
};
template <>
struct ToNodeTypeImpl<NodeType::ParameterDeclaration> {
    using type = ParameterDeclaration;
};
template <>
struct ToNodeTypeImpl<NodeType::ExpressionStatement> {
    using type = ExpressionStatement;
};
template <>
struct ToNodeTypeImpl<NodeType::EmptyStatement> {
    using type = EmptyStatement;
};
template <>
struct ToNodeTypeImpl<NodeType::ReturnStatement> {
    using type = ReturnStatement;
};
template <>
struct ToNodeTypeImpl<NodeType::IfStatement> {
    using type = IfStatement;
};
template <>
struct ToNodeTypeImpl<NodeType::WhileStatement> {
    using type = WhileStatement;
};
template <>
struct ToNodeTypeImpl<NodeType::Identifier> {
    using type = Identifier;
};
template <>
struct ToNodeTypeImpl<NodeType::IntegerLiteral> {
    using type = IntegerLiteral;
};
template <>
struct ToNodeTypeImpl<NodeType::BooleanLiteral> {
    using type = BooleanLiteral;
};
template <>
struct ToNodeTypeImpl<NodeType::FloatingPointLiteral> {
    using type = FloatingPointLiteral;
};
template <>
struct ToNodeTypeImpl<NodeType::StringLiteral> {
    using type = StringLiteral;
};
template <>
struct ToNodeTypeImpl<NodeType::UnaryPrefixExpression> {
    using type = UnaryPrefixExpression;
};
template <>
struct ToNodeTypeImpl<NodeType::BinaryExpression> {
    using type = BinaryExpression;
};
template <>
struct ToNodeTypeImpl<NodeType::MemberAccess> {
    using type = MemberAccess;
};
template <>
struct ToNodeTypeImpl<NodeType::Conditional> {
    using type = Conditional;
};
template <>
struct ToNodeTypeImpl<NodeType::FunctionCall> {
    using type = FunctionCall;
};
template <>
struct ToNodeTypeImpl<NodeType::Subscript> {
    using type = Subscript;
};
} // namespace internal

template <typename T>
inline constexpr NodeType ToEnumNodeType = internal::ToEnumNodeTypeImpl<T>::value;
template <NodeType E>
using ToNodeType = typename internal::ToNodeTypeImpl<E>::type;

template <std::derived_from<AbstractSyntaxTree> ConcreteNode>
constexpr ConcreteNode& downCast(std::derived_from<AbstractSyntaxTree> auto& node) {
    return *downCast<ConcreteNode>(&node);
}
template <std::derived_from<AbstractSyntaxTree> ConcreteNode>
constexpr ConcreteNode const& downCast(std::derived_from<AbstractSyntaxTree> auto const& node) {
    return *downCast<ConcreteNode>(&node);
}

template <std::derived_from<AbstractSyntaxTree> ConcreteNode>
constexpr ConcreteNode* downCast(std::derived_from<AbstractSyntaxTree> auto* node) {
    return const_cast<ConcreteNode*>(downCast<ConcreteNode>(&std::as_const(*node)));
}
template <std::derived_from<AbstractSyntaxTree> ConcreteNode>
constexpr ConcreteNode const* downCast(std::derived_from<AbstractSyntaxTree> auto const* node) {
    SC_ASSERT(node->nodeType() == ToEnumNodeType<ConcreteNode>, "Wrong type");
    return static_cast<ConcreteNode const*>(node);
}

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

enum class EntityCategory { Value, Type, _count };

SCATHA(API) std::ostream& operator<<(std::ostream&, EntityCategory);

} // namespace scatha::ast

#endif // SCATHA_AST_COMMON_H_
