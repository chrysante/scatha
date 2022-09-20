#ifndef SCATHA_AST_EXPRESSION_H_
#define SCATHA_AST_EXPRESSION_H_

#include <iosfwd>
#include <string>

#include <utl/static_string.hpp>

#include "AST/Base.h"
#include "AST/Common.h"
#include "Sema/SemanticElements.h"

namespace scatha::ast {

	/// Abstract node representing any declaration.
	struct Expression: AbstractSyntaxTree {
		using AbstractSyntaxTree::AbstractSyntaxTree;
		
		/** Decoration provided by semantic analysis. */
		
		/// The type of the expression.
		sema::TypeID typeID{};
	};
	
	/// MARK: Nullary Expressions
	
	/// Concrete node representing an identifier in an expression.
	/// Identifier must refer to a value meaning a variable or a function.
	struct Identifier: Expression {
		explicit Identifier(Token const& token):
			Expression(NodeType::Identifier, token)
		{}
		
		std::string_view value() const { return token().id; }
		
		/** Decoration provided by semantic analysis. */
		
		sema::SymbolID symbolID;
	};
	
	struct IntegerLiteral: Expression {
		explicit IntegerLiteral(Token const& token):
			Expression(NodeType::IntegerLiteral, token),
			value(token.toInteger())
		{}
		
		u64 value;
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
		explicit MemberAccess(UniquePtr<Expression> object, Token const& memberToken, Token const& dotToken):
			Expression(NodeType::MemberAccess, dotToken),
			object(std::move(object)),
			_member(memberToken)
		{}
		
		Token const& member() const { return _member; }
		std::string_view memberName() const { return _member.id; }
		
		UniquePtr<Expression> object;
		
	private:
		Token _member;
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

