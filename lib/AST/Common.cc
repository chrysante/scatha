#include "AST/Common.h"

#include <utl/utility.hpp>

namespace scatha::ast {
	
	std::string_view toString(NodeType t) {
		using enum NodeType;
		return UTL_SERIALIZE_ENUM(t, {
			{ NodeType::TranslationUnit,       "TranslationUnit" },
			{ NodeType::Block,                 "Block" },
			{ NodeType::FunctionDeclaration,   "FunctionDeclaration" },
			{ NodeType::FunctionDefinition,    "FunctionDefinition" },
			{ NodeType::StructDeclaration,     "StructDeclaration" },
			{ NodeType::StructDefinition,      "StructDefinition" },
			{ NodeType::VariableDeclaration,   "VariableDeclaration" },
			{ NodeType::ExpressionStatement,   "ExpressionStatement" },
			{ NodeType::ReturnStatement,       "ReturnStatement" },
			{ NodeType::IfStatement,           "IfStatement" },
			{ NodeType::WhileStatement,        "WhileStatement" },
			{ NodeType::Identifier,            "Identifier" },
			{ NodeType::IntegerLiteral,        "IntegerLiteral" },
			{ NodeType::BooleanLiteral,        "BooleanLiteral" },
			{ NodeType::FloatingPointLiteral,  "FloatingPointLiteral" },
			{ NodeType::StringLiteral,         "StringLiteral" },
			{ NodeType::UnaryPrefixExpression, "UnaryPrefixExpression" },
			{ NodeType::BinaryExpression,      "BinaryExpression" },
			{ NodeType::MemberAccess,          "MemberAccess" },
			{ NodeType::Conditional,           "Conditional" },
			{ NodeType::FunctionCall,          "FunctionCall" },
			{ NodeType::Subscript,             "Subscript" },
		});
	}
	
	std::string_view toString(UnaryPrefixOperator op) {
		using enum UnaryPrefixOperator;
		return UTL_SERIALIZE_ENUM(op, {
			{ UnaryPrefixOperator::Promotion,  "+" },
			{ UnaryPrefixOperator::Negation,   "-" },
			{ UnaryPrefixOperator::BitwiseNot, "~" },
			{ UnaryPrefixOperator::LogicalNot, "!" },
		});
	}
	
	std::string_view toString(BinaryOperator op) {
		using enum BinaryOperator;
		return UTL_SERIALIZE_ENUM(op, {
			{ BinaryOperator::Multiplication, "*" },
			{ BinaryOperator::Division,       "/" },
			{ BinaryOperator::Remainder,      "%" },
			{ BinaryOperator::Addition,       "+" },
			{ BinaryOperator::Subtraction,    "-" },
			{ BinaryOperator::LeftShift,      "<<" },
			{ BinaryOperator::RightShift,     ">>" },
			{ BinaryOperator::Less,           "<" },
			{ BinaryOperator::LessEq,         "<=" },
			{ BinaryOperator::Greater,        ">" },
			{ BinaryOperator::GreaterEq,      ">=" },
			{ BinaryOperator::Equals,         "==" },
			{ BinaryOperator::NotEquals,      "!=" },
			{ BinaryOperator::BitwiseAnd,     "&" },
			{ BinaryOperator::BitwiseXOr,     "^" },
			{ BinaryOperator::BitwiseOr,      "|" },
			{ BinaryOperator::LogicalAnd,     "&&" },
			{ BinaryOperator::LogicalOr,      "||" },
			{ BinaryOperator::Assignment,     "=" },
			{ BinaryOperator::AddAssignment,  "+=" },
			{ BinaryOperator::SubAssignment,  "-=" },
			{ BinaryOperator::MulAssignment,  "*=" },
			{ BinaryOperator::DivAssignment,  "/=" },
			{ BinaryOperator::RemAssignment,  "%=" },
			{ BinaryOperator::LSAssignment,   "<<=" },
			{ BinaryOperator::RSAssignment,   ">>=" },
			{ BinaryOperator::AndAssignment,  "&=" },
			{ BinaryOperator::OrAssignment,   "|=" },
			{ BinaryOperator::Comma,          "," },
		});
		
	}
	
}
