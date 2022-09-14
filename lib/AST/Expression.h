#ifndef SCATHA_AST_EXPRESSION_H_
#define SCATHA_AST_EXPRESSION_H_

#include <iosfwd>
#include <string>

#include <utl/static_string.hpp>

#include "AST/ASTBase.h"
#include "AST/Operator.h"

#include "Common/Type.h"

namespace scatha::ast {

	struct Expression: AbstractSyntaxTree {
		using AbstractSyntaxTree::AbstractSyntaxTree;
		TypeID typeID{};
	};
	
	/// MARK: Nullary Expressions
	struct Identifier: Expression {
		explicit Identifier(Token const& token):
			Expression(NodeType::Identifier, token),
			value(token.id)
		{}
		
		std::string value;
	};
	
	struct NumericLiteral: Expression {
		explicit NumericLiteral(Token const& token):
			Expression(NodeType::NumericLiteral, token),
			value(token.id)
		{}
		
		std::string value;
	};
	
	struct StringLiteral: Expression {
		explicit StringLiteral(Token const& token):
			Expression(NodeType::StringLiteral, token),
			value(token.id)
		{}
		
		std::string value;
	};
	
	/// MARK: Unary Expressions
	struct UnaryPrefixExpression: Expression {
		explicit UnaryPrefixExpression(UnaryPrefixOperator op, UniquePtr<Expression> operand, Token const& token):
			Expression(NodeType::UnaryPrefixExpression, token),
			op(op),
			operand(std::move(operand))
		{}
		
		UnaryPrefixOperator op;
		UniquePtr<Expression> operand;
	};
	
	/// MARK: Binary Expressions
	struct BinaryExpression: Expression {
		explicit BinaryExpression(BinaryOperator op, UniquePtr<Expression> lhs, UniquePtr<Expression> rhs, Token const& token):
			Expression(NodeType::BinaryExpression, token),
			op(op),
			lhs(std::move(lhs)),
			rhs(std::move(rhs))
		{}
		
		
		BinaryOperator op;
		UniquePtr<Expression> lhs;
		UniquePtr<Expression> rhs;
	};
	
	struct MemberAccess: Expression {
		explicit MemberAccess(UniquePtr<Expression> object, UniquePtr<Identifier> member, Token const& token):
			Expression(NodeType::MemberAccess, token),
			object(std::move(object)),
			member(std::move(member))
		{}
		
		UniquePtr<Expression> object;
		UniquePtr<Identifier> member;
	};
	
	/// MARK: Ternary Expressions
	struct Conditional: Expression {
		explicit Conditional(UniquePtr<Expression> condition, UniquePtr<Expression> ifExpr, UniquePtr<Expression> elseExpr,
							 Token const& token):
			Expression(NodeType::Conditional, token),
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
		explicit FunctionCall(UniquePtr<Expression> object, Token const& token):
			Expression(NodeType::FunctionCall, token),
			object(std::move(object))
		{}
		
		UniquePtr<Expression> object;
		utl::small_vector<UniquePtr<Expression>> arguments;
	};
	
	struct Subscript: Expression {
		explicit Subscript(UniquePtr<Expression> object, Token const& token):
			Expression(NodeType::Subscript, token),
			object(std::move(object))
		{}
		
		UniquePtr<Expression> object;
		utl::small_vector<UniquePtr<Expression>> arguments;
	};
	
}


#endif // SCATHA_AST_EXPRESSION_H_

