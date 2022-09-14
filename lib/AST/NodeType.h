#ifndef SCATHA_AST_NODETYPE_H_
#define SCATHA_AST_NODETYPE_H_

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
	
}

#endif // SCATHA_AST_NODETYPE_H_

