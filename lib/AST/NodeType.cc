#include "NodeType.h"

#include <utl/utility.hpp>

namespace scatha::ast {
	
	std::string_view toString(NodeType t) {
		using enum NodeType;
		return UTL_SERIALIZE_ENUM(t, {
			{ TranslationUnit,       "TranslationUnit" },
			
			{ Block,                 "Block" },
			{ FunctionDeclaration,   "FunctionDeclaration" },
			{ FunctionDefinition,    "FunctionDefinition" },
			{ VariableDeclaration,   "VariableDeclaration" },
			{ ExpressionStatement,   "ExpressionStatement" },
			{ ReturnStatement,       "ReturnStatement" },
			{ IfStatement,           "IfStatement" },
			{ WhileStatement,        "WhileStatement" },
			
			{ Identifier,            "Identifier" },
			{ NumericLiteral,        "NumericLiteral" },
			{ StringLiteral,         "StringLiteral" },
			
			{ UnaryPrefixExpression, "UnaryPrefixExpression" },
			
			{ BinaryExpression,      "BinaryExpression" },
			{ MemberAccess,          "MemberAccess" },
			
			{ Conditional,           "Conditional" },
			
			{ FunctionCall,          "FunctionCall" },
			{ Subscript,             "Subscript" },
		});
	}
	
}
