#ifndef SCATHA_AST_COMMON_H_
#define SCATHA_AST_COMMON_H_

#include <string_view>

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
		VariableDeclaration,
		ExpressionStatement,
		ReturnStatement,
		IfStatement,
		WhileStatement,
		
		// Expressions
		
		// Nullary Expressions
		Identifier,
		NumericLiteral,
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
	
	std::string_view toString(NodeType);
	
	
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

	std::string_view toString(UnaryPrefixOperator);
	
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
	
	std::string_view toString(BinaryOperator);
	
}

#endif // SCATHA_AST_COMMON_H_

