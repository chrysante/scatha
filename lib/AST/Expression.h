#ifndef SCATHA_AST_EXPRESSION_H_
#define SCATHA_AST_EXPRESSION_H_

#include <iosfwd>
#include <string>

#include <utl/static_string.hpp>
#include <utl/vector.hpp>

#include "AST/Base.h"
#include "AST/Common.h"
#include "Sema/SymbolID.h"

namespace scatha::ast {

	/// Abstract node representing any declaration.
	struct SCATHA(API) Expression: AbstractSyntaxTree {
		using AbstractSyntaxTree::AbstractSyntaxTree;
		
		bool isValue() const { return kind == ExpressionKind::Value; }
		bool isType() const { return kind == ExpressionKind::Type; }
		
		/** Decoration provided by semantic analysis. */
		
		/// Kind of the expression. Either ::Value or ::Type. ::Value by default.
		ExpressionKind kind = ExpressionKind::Value;
		
		/// The type of the expression. Only valid if kind == ::Value
		sema::TypeID typeID{};
	};
	
	/// MARK: Nullary Expressions
	
	/// Concrete node representing an identifier in an expression.
	/// Identifier must refer to a value meaning a variable or a function.
	struct SCATHA(API) Identifier: Expression {
		explicit Identifier(Token const& token):
			Expression(NodeType::Identifier, token)
		{}
		
		std::string_view value() const { return token().id; }
		
		/** Decoration provided by semantic analysis. */
		
		sema::SymbolID symbolID;
	};
	
	struct SCATHA(API) IntegerLiteral: Expression {
		explicit IntegerLiteral(Token const& token):
			Expression(NodeType::IntegerLiteral, token),
			value(token.toInteger())
		{}
		
		u64 value;
	};
	
	struct SCATHA(API) BooleanLiteral: Expression {
		explicit BooleanLiteral(Token const& token):
			Expression(NodeType::BooleanLiteral, token),
			value(token.toBool())
		{}
		
		bool value;
	};
	
	struct SCATHA(API) FloatingPointLiteral: Expression {
		explicit FloatingPointLiteral(Token const& token):
			Expression(NodeType::FloatingPointLiteral, token),
			value(token.toFloat())
		{}
		
		f64 value;
	};
	
	struct SCATHA(API) StringLiteral: Expression {
		explicit StringLiteral(Token const& token):
			Expression(NodeType::StringLiteral, token),
			value(token.id)
		{}
		
		std::string value;
	};
	
	/// MARK: Unary Expressions
	struct SCATHA(API) UnaryPrefixExpression: Expression {
		explicit UnaryPrefixExpression(UnaryPrefixOperator op, UniquePtr<Expression> operand, Token const& token):
			Expression(NodeType::UnaryPrefixExpression, token),
			op(op),
			operand(std::move(operand))
		{}
		
		UnaryPrefixOperator op;
		UniquePtr<Expression> operand;
	};
	
	/// MARK: Binary Expressions
	struct SCATHA(API) BinaryExpression: Expression {
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
	
	struct SCATHA(API) MemberAccess: Expression {
		explicit MemberAccess(UniquePtr<Expression> object, UniquePtr<Expression> member, Token const& dotToken):
			Expression(NodeType::MemberAccess, dotToken),
			object(std::move(object)),
			member(std::move(member))
		{}
		
		UniquePtr<Expression> object;
		UniquePtr<Expression> member;
		sema::SymbolID symbolID;
	};
	
	/// MARK: Ternary Expressions
	struct SCATHA(API) Conditional: Expression {
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
	struct SCATHA(API) FunctionCall: Expression {
		explicit FunctionCall(UniquePtr<Expression> object, Token const& token):
			Expression(NodeType::FunctionCall, token),
			object(std::move(object))
		{}
		
		UniquePtr<Expression> object;
		utl::small_vector<UniquePtr<Expression>> arguments;
	};
	
	struct SCATHA(API) Subscript: Expression {
		explicit Subscript(UniquePtr<Expression> object, Token const& token):
			Expression(NodeType::Subscript, token),
			object(std::move(object))
		{}
		
		UniquePtr<Expression> object;
		utl::small_vector<UniquePtr<Expression>> arguments;
	};
	
}


#endif // SCATHA_AST_EXPRESSION_H_

