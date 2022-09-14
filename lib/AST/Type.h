#ifndef SCATHA_AST_TYPE_H_
#define SCATHA_AST_TYPE_H_

namespace scatha::ast {
	
	/**
	 * List of all concrete AST node types
	 */
	enum class Type {
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
		
		// Binary Expressions
		BinaryExpression,
		MemberAccess,
		
		// Ternary Expressions
		Conditional,
		
		// More Complex Expressions
		FunctionCall,
		Subscript,
	};
	
}

#endif // SCATHA_AST_TYPE_H_

