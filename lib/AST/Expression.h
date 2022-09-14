#ifndef SCATHA_AST_EXPRESSION_H_
#define SCATHA_AST_EXPRESSION_H_

#include <iosfwd>
#include <string>

#include <utl/static_string.hpp>

#include "AST/AST.h"
#include "AST/Operator.h"

namespace scatha::ast {

	struct Expression: AbstractSyntaxTree {
		using AbstractSyntaxTree::AbstractSyntaxTree;
	};
	
	/// MARK: Nullary Expressions
	struct Identifier: Expression {
		explicit Identifier(std::string name):
			Expression(NodeType::Identifier),
			name(std::move(name))
		{}
		
		std::string name;
	};
	
	struct NumericLiteral: Expression {
		explicit NumericLiteral(std::string value):
			Expression(NodeType::NumericLiteral),
			value(std::move(value))
		{}
		
		std::string value;
	};
	
	struct StringLiteral: Expression {
		explicit StringLiteral(std::string value):
			Expression(NodeType::StringLiteral),
			value(std::move(value))
		{}
		
		std::string value;
	};
	
	/// MARK: Unary Expressions
	struct UnaryPrefixExpression: Expression {
		explicit UnaryPrefixExpression(UnaryPrefixOperator op, UniquePtr<Expression> operand):
			Expression(NodeType::UnaryPrefixExpression),
			op(op),
			operand(std::move(operand))
		{}
		
		UnaryPrefixOperator op;
		UniquePtr<Expression> operand;
	};
	
	/// MARK: Binary Expressions
	struct BinaryExpression: Expression {
		explicit BinaryExpression(BinaryOperator op, UniquePtr<Expression> lhs, UniquePtr<Expression> rhs):
			Expression(NodeType::BinaryExpression),
			op(op),
			lhs(std::move(lhs)),
			rhs(std::move(rhs))
		{}
		
		
		BinaryOperator op;
		UniquePtr<Expression> lhs;
		UniquePtr<Expression> rhs;
	};
	
	struct MemberAccess: Expression {
		explicit MemberAccess(UniquePtr<Expression> object, std::string memberID):
			Expression(NodeType::MemberAccess),
			object(std::move(object)),
			memberID(std::move(memberID))
		{}
		
		UniquePtr<Expression> object;
		std::string memberID;
	};
	
	/// MARK: Ternary Expressions
	struct Conditional: Expression {
		explicit Conditional(UniquePtr<Expression> condition, UniquePtr<Expression> ifExpr, UniquePtr<Expression> elseExpr):
			Expression(NodeType::Conditional),
			condition(std::move(condition)),
			ifExpr(std::move(ifExpr)),
			elseExpr(std::move(elseExpr))
		{}
		
		UniquePtr<Expression> condition;
		UniquePtr<Expression> ifExpr;
		UniquePtr<Expression> elseExpr;
	};
	
	/// MARK: More Complex Expressions
	struct FunctionCall: Expression {
		explicit FunctionCall(UniquePtr<Expression> object, utl::vector<UniquePtr<Expression>> arguments = {}):
			Expression(NodeType::FunctionCall),
			object(std::move(object)),
			arguments(std::move(arguments))
		{}
		
		UniquePtr<Expression> object;
		utl::small_vector<UniquePtr<Expression>> arguments;
	};
	
	struct Subscript: Expression {
		explicit Subscript(UniquePtr<Expression> object, utl::vector<UniquePtr<Expression>> arguments = {}):
			Expression(NodeType::Subscript),
			object(std::move(object)),
			arguments(std::move(arguments))
		{}
		
		UniquePtr<Expression> object;
		utl::small_vector<UniquePtr<Expression>> arguments;
	};
	
}


#endif // SCATHA_AST_EXPRESSION_H_

