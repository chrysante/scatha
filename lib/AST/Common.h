#ifndef SCATHA_AST_COMMON_H_
#define SCATHA_AST_COMMON_H_

#include <concepts>
#include <string_view>
#include <iosfwd>

#include "AST/Fwd.h"
#include "Basic/Basic.h"

namespace scatha::ast {
	
	/**
	 * List of all concrete AST node types
	 */
	enum class NodeType {
		TranslationUnit,
		
		// Statements
		Block,
		FunctionDeclaration,
		FunctionDefinition,
		StructDeclaration,
		StructDefinition,
		VariableDeclaration,
		ExpressionStatement,
		ReturnStatement,
		IfStatement,
		WhileStatement,
		
		// Expressions
		
		// Nullary Expressions
		Identifier,
		IntegerLiteral,
		BooleanLiteral,
		FloatingPointLiteral,
		StringLiteral,
		
		// Unary Expressions
		UnaryPrefixExpression,
		
		// Binary Expressions
		BinaryExpression,
		MemberAccess,
		
		// Ternary Expressions
		Conditional,
		
		// More Complex Expressions
		FunctionCall,
		Subscript,
		
		_count
	};
	
	SCATHA(API) std::string_view toString(NodeType);
	
	SCATHA(API) std::ostream& operator<<(std::ostream&, NodeType);
	
	namespace internal {
		template <typename> struct ToEnumNodeTypeImpl;
		template <> struct ToEnumNodeTypeImpl<TranslationUnit>:       std::integral_constant<NodeType, NodeType::TranslationUnit> {};
		template <> struct ToEnumNodeTypeImpl<Block>:                 std::integral_constant<NodeType, NodeType::Block> {};
		template <> struct ToEnumNodeTypeImpl<FunctionDeclaration>:   std::integral_constant<NodeType, NodeType::FunctionDeclaration> {};
		template <> struct ToEnumNodeTypeImpl<FunctionDefinition>:    std::integral_constant<NodeType, NodeType::FunctionDefinition> {};
		template <> struct ToEnumNodeTypeImpl<StructDeclaration>:     std::integral_constant<NodeType, NodeType::StructDeclaration> {};
		template <> struct ToEnumNodeTypeImpl<StructDefinition>:      std::integral_constant<NodeType, NodeType::StructDefinition> {};
		template <> struct ToEnumNodeTypeImpl<VariableDeclaration>:   std::integral_constant<NodeType, NodeType::VariableDeclaration> {};
		template <> struct ToEnumNodeTypeImpl<ExpressionStatement>:   std::integral_constant<NodeType, NodeType::ExpressionStatement> {};
		template <> struct ToEnumNodeTypeImpl<ReturnStatement>:       std::integral_constant<NodeType, NodeType::ReturnStatement> {};
		template <> struct ToEnumNodeTypeImpl<IfStatement>:           std::integral_constant<NodeType, NodeType::IfStatement> {};
		template <> struct ToEnumNodeTypeImpl<WhileStatement>:        std::integral_constant<NodeType, NodeType::WhileStatement> {};
		template <> struct ToEnumNodeTypeImpl<Identifier>:            std::integral_constant<NodeType, NodeType::Identifier> {};
		template <> struct ToEnumNodeTypeImpl<IntegerLiteral>:        std::integral_constant<NodeType, NodeType::IntegerLiteral> {};
		template <> struct ToEnumNodeTypeImpl<BooleanLiteral>:        std::integral_constant<NodeType, NodeType::BooleanLiteral> {};
		template <> struct ToEnumNodeTypeImpl<FloatingPointLiteral>:  std::integral_constant<NodeType, NodeType::FloatingPointLiteral> {};
		template <> struct ToEnumNodeTypeImpl<StringLiteral>:         std::integral_constant<NodeType, NodeType::StringLiteral> {};
		template <> struct ToEnumNodeTypeImpl<UnaryPrefixExpression>: std::integral_constant<NodeType, NodeType::UnaryPrefixExpression> {};
		template <> struct ToEnumNodeTypeImpl<BinaryExpression>:      std::integral_constant<NodeType, NodeType::BinaryExpression> {};
		template <> struct ToEnumNodeTypeImpl<MemberAccess>:          std::integral_constant<NodeType, NodeType::MemberAccess> {};
		template <> struct ToEnumNodeTypeImpl<Conditional>:           std::integral_constant<NodeType, NodeType::Conditional> {};
		template <> struct ToEnumNodeTypeImpl<FunctionCall>:          std::integral_constant<NodeType, NodeType::FunctionCall> {};
		template <> struct ToEnumNodeTypeImpl<Subscript>:             std::integral_constant<NodeType, NodeType::Subscript> {};
		
		template <NodeType> struct ToNodeTypeImpl;
		template <> struct ToNodeTypeImpl<NodeType::TranslationUnit>       { using type = TranslationUnit; };
		template <> struct ToNodeTypeImpl<NodeType::Block>                 { using type = Block; };
		template <> struct ToNodeTypeImpl<NodeType::FunctionDeclaration>   { using type = FunctionDeclaration; };
		template <> struct ToNodeTypeImpl<NodeType::FunctionDefinition>    { using type = FunctionDefinition; };
		template <> struct ToNodeTypeImpl<NodeType::StructDeclaration>     { using type = StructDeclaration; };
		template <> struct ToNodeTypeImpl<NodeType::StructDefinition>      { using type = StructDefinition; };
		template <> struct ToNodeTypeImpl<NodeType::VariableDeclaration>   { using type = VariableDeclaration; };
		template <> struct ToNodeTypeImpl<NodeType::ExpressionStatement>   { using type = ExpressionStatement; };
		template <> struct ToNodeTypeImpl<NodeType::ReturnStatement>       { using type = ReturnStatement; };
		template <> struct ToNodeTypeImpl<NodeType::IfStatement>           { using type = IfStatement; };
		template <> struct ToNodeTypeImpl<NodeType::WhileStatement>        { using type = WhileStatement; };
		template <> struct ToNodeTypeImpl<NodeType::Identifier>            { using type = Identifier; };
		template <> struct ToNodeTypeImpl<NodeType::IntegerLiteral>        { using type = IntegerLiteral; };
		template <> struct ToNodeTypeImpl<NodeType::BooleanLiteral>        { using type = BooleanLiteral; };
		template <> struct ToNodeTypeImpl<NodeType::FloatingPointLiteral>  { using type = FloatingPointLiteral; };
		template <> struct ToNodeTypeImpl<NodeType::StringLiteral>         { using type = StringLiteral; };
		template <> struct ToNodeTypeImpl<NodeType::UnaryPrefixExpression> { using type = UnaryPrefixExpression; };
		template <> struct ToNodeTypeImpl<NodeType::BinaryExpression>      { using type = BinaryExpression; };
		template <> struct ToNodeTypeImpl<NodeType::MemberAccess>          { using type = MemberAccess; };
		template <> struct ToNodeTypeImpl<NodeType::Conditional>           { using type = Conditional; };
		template <> struct ToNodeTypeImpl<NodeType::FunctionCall>          { using type = FunctionCall; };
		template <> struct ToNodeTypeImpl<NodeType::Subscript>             { using type = Subscript; };
	}
	template <typename T>
	inline constexpr NodeType ToEnumNodeType = internal::ToEnumNodeTypeImpl<T>::value;
	template <NodeType E>
	using ToNodeType = typename internal::ToNodeTypeImpl<E>::type;
	
	template <std::derived_from<AbstractSyntaxTree> ConcreteNode>
	constexpr ConcreteNode& downCast(std::derived_from<AbstractSyntaxTree> auto& node)
	{ return downCast<ConcreteNode>(&node); }
	template <std::derived_from<AbstractSyntaxTree> ConcreteNode>
	constexpr ConcreteNode const& downCast(std::derived_from<AbstractSyntaxTree> auto const& node)
	{ return downCast<ConcreteNode>(&node); }
	
	template <std::derived_from<AbstractSyntaxTree> ConcreteNode>
	constexpr ConcreteNode* downCast(std::derived_from<AbstractSyntaxTree> auto* node)
	{ return const_cast<ConcreteNode*>(downCast<ConcreteNode>(&std::as_const(*node))); }
	template <std::derived_from<AbstractSyntaxTree> ConcreteNode>
	constexpr ConcreteNode const* downCast(std::derived_from<AbstractSyntaxTree> auto const* node) {
		SC_ASSERT(node->nodeType() == ToEnumNodeType<ConcreteNode>, "Wrong type");
		return static_cast<ConcreteNode const*>(node);
	}
	
	/**
	 * List of all unary operators in prefix notation
	 */
	enum class UnaryPrefixOperator {
		Promotion,
		Negation,
		BitwiseNot,
		LogicalNot,
		
		_count
	};

	SCATHA(API) std::string_view toString(UnaryPrefixOperator);
	
	SCATHA(API) std::ostream& operator<<(std::ostream&, UnaryPrefixOperator);
	
	/**
	 * List of all binary operators in infix notation
	 */
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
		
		Comma,
		
		_count
	};
	
	SCATHA(API) std::string_view toString(BinaryOperator);

	SCATHA(API) std::ostream& operator<<(std::ostream&, BinaryOperator);
	
}

#endif // SCATHA_AST_COMMON_H_

