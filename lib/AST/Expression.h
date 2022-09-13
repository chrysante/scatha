#ifndef SCATHA_AST_EXPRESSION_H_
#define SCATHA_AST_EXPRESSION_H_

#include <iosfwd>
#include <string>

#include <utl/static_string.hpp>

#include "AST/AST.h"

namespace scatha::ast {

	struct Expression: AbstractSyntaxTree {

	};
	
	namespace internal {
	
		struct UnaryPrefixExprBase {
			UniquePtr<Expression> operand;
			
			void printImpl(std::ostream&, AbstractSyntaxTree::Indenter&, std::string_view op) const;
		};
		
		struct BinaryExprBase {
			UniquePtr<Expression> left;
			UniquePtr<Expression> right;
			
			void printImpl(std::ostream&, AbstractSyntaxTree::Indenter&, std::string_view op) const;
		};
	
	}
		
	/**
	 * Not meant to be used directly. Only as base class of actual expressions.
	 */
	template <utl::basic_static_string OperatorString>
	struct UnaryPrefixExpression: Expression, internal::UnaryPrefixExprBase {
		explicit UnaryPrefixExpression(UniquePtr<Expression> operand):
			internal::UnaryPrefixExprBase{ std::move(operand) }
		{}
		
		static constexpr std::string_view operatorString() { return OperatorString; }
		
		void print(std::ostream& str, Indenter& indent) const override {
			this->printImpl(str, indent, operatorString());
		}
	};
	
	template <utl::basic_static_string OperatorString>
	struct BinaryExpression: Expression, internal::BinaryExprBase {
		explicit BinaryExpression(UniquePtr<Expression> left, UniquePtr<Expression> right):
			internal::BinaryExprBase{ std::move(left), std::move(right) }
		{}
		
		static constexpr std::string_view operatorString() { return OperatorString; }
		
		void print(std::ostream& str, Indenter& indent) const override {
			this->printImpl(str, indent, operatorString());
		}
	};
	
	struct Identifier: Expression {
		explicit Identifier(std::string name):
			name(std::move(name))
		{}
		
		void print(std::ostream&, Indenter&) const override;
		
		std::string name;
	};
	
	struct NumericLiteral: Expression {
		explicit NumericLiteral(std::string value):
			value(std::move(value))
		{}
		
		void print(std::ostream&, Indenter&) const override;
		
		std::string value;
	};
	
	struct StringLiteral: Expression {
		explicit StringLiteral(std::string value):
			value(std::move(value))
		{}
		
		void print(std::ostream&, Indenter&) const override;
		
		std::string value;
	};
	
	struct Addition: BinaryExpression<"+"> {
		using BinaryExpression<"+">::BinaryExpression;
	};
	
	struct Subtraction: BinaryExpression<"-"> {
		using BinaryExpression<"-">::BinaryExpression;
	};
	
	struct Promotion: UnaryPrefixExpression<"+"> {
		using UnaryPrefixExpression<"+">::UnaryPrefixExpression;
	};
	
	struct Negation: UnaryPrefixExpression<"-"> {
		using UnaryPrefixExpression<"-">::UnaryPrefixExpression;
	};
	
	struct BitwiseNot: UnaryPrefixExpression<"~"> {
		using UnaryPrefixExpression<"~">::UnaryPrefixExpression;
	};
	
	struct LogicalNot: UnaryPrefixExpression<"!"> {
		using UnaryPrefixExpression<"!">::UnaryPrefixExpression;
	};
	
	struct Comma: BinaryExpression<","> {
		using BinaryExpression<",">::BinaryExpression;
	};
	
	struct Multiplication: BinaryExpression<"*"> {
		using BinaryExpression<"*">::BinaryExpression;
	};
	
	struct Division: BinaryExpression<"/"> {
		using BinaryExpression<"/">::BinaryExpression;
	};
	
	struct Remainder: BinaryExpression<"%"> {
		using BinaryExpression<"%">::BinaryExpression;
	};
	
	struct LogicalOr: BinaryExpression<"||"> {
		using BinaryExpression<"||">::BinaryExpression;
	};
	
	struct LogicalAnd: BinaryExpression<"&&"> {
		using BinaryExpression<"&&">::BinaryExpression;
	};
	
	struct BitwiseOr: BinaryExpression<"|"> {
		using BinaryExpression<"|">::BinaryExpression;
	};
	
	struct BitwiseXOr: BinaryExpression<"^"> {
		using BinaryExpression<"^">::BinaryExpression;
	};
	
	struct BitwiseAnd: BinaryExpression<"&"> {
		using BinaryExpression<"&">::BinaryExpression;
	};
	
	struct Equals: BinaryExpression<"=="> {
		using BinaryExpression<"==">::BinaryExpression;
	};
	
	struct NotEquals: BinaryExpression<"!="> {
		using BinaryExpression<"!=">::BinaryExpression;
	};
	
	struct Less: BinaryExpression<"<"> {
		using BinaryExpression<"<">::BinaryExpression;
	};
	
	struct LessEq: BinaryExpression<"<="> {
		using BinaryExpression<"<=">::BinaryExpression;
	};
	
	struct Greater: BinaryExpression<">"> {
		using BinaryExpression<">">::BinaryExpression;
	};
	
	struct GreaterEq: BinaryExpression<">="> {
		using BinaryExpression<">=">::BinaryExpression;
	};
	
	struct LeftShift: BinaryExpression<"<<"> {
		using BinaryExpression<"<<">::BinaryExpression;
	};
	
	struct RightShift: BinaryExpression<">>"> {
		using BinaryExpression<">>">::BinaryExpression;
	};
	
	struct Assignment: BinaryExpression<"="> {
		using BinaryExpression<"=">::BinaryExpression;
	};
	
	struct AddAssignment: BinaryExpression<"+="> {
		using BinaryExpression<"+=">::BinaryExpression;
	};
	
	struct SubAssignment: BinaryExpression<"-="> {
		using BinaryExpression<"-=">::BinaryExpression;
	};
	
	struct MulAssignment: BinaryExpression<"*="> {
		using BinaryExpression<"*=">::BinaryExpression;
	};
	
	struct DivAssignment: BinaryExpression<"/="> {
		using BinaryExpression<"/=">::BinaryExpression;
	};
	
	struct RemAssignment: BinaryExpression<"%="> {
		using BinaryExpression<"%=">::BinaryExpression;
	};
	
	struct LSAssignment: BinaryExpression<"<<="> {
		using BinaryExpression<"<<=">::BinaryExpression;
	};
	
	struct RSAssignment: BinaryExpression<">>="> {
		using BinaryExpression<">>=">::BinaryExpression;
	};
	
	struct AndAssignment: BinaryExpression<"&="> {
		using BinaryExpression<"&=">::BinaryExpression;
	};
	
	struct OrAssignment: BinaryExpression<"|="> {
		using BinaryExpression<"|=">::BinaryExpression;
	};
	
	struct FunctionCall: Expression {
		explicit FunctionCall(UniquePtr<Expression> object, utl::vector<UniquePtr<Expression>> arguments = {}):
			object(std::move(object)),
			arguments(std::move(arguments))
		{}
		void print(std::ostream&, Indenter&) const override;
		
		UniquePtr<Expression> object;
		utl::small_vector<UniquePtr<Expression>> arguments;
	};
	
	struct Subscript: Expression {
		explicit Subscript(UniquePtr<Expression> object, utl::vector<UniquePtr<Expression>> arguments = {}):
			object(std::move(object)),
			arguments(std::move(arguments))
		{}
		void print(std::ostream&, Indenter&) const override;
		
		UniquePtr<Expression> object;
		utl::small_vector<UniquePtr<Expression>> arguments;
	};
	
	struct MemberAccess: Expression {
		explicit MemberAccess(UniquePtr<Expression> object, std::string memberID):
			object(std::move(object)),
			memberID(std::move(memberID))
		{}
		
		void print(std::ostream&, Indenter&) const override;
		
		UniquePtr<Expression> object;
		std::string memberID;
	};
	
	struct Conditional: Expression {
		explicit Conditional(UniquePtr<Expression> condition, UniquePtr<Expression> if_, UniquePtr<Expression> then):
			condition(std::move(condition)),
			if_(std::move(if_)),
			then(std::move(then))
		{}
		
		void print(std::ostream&, Indenter&) const override;
		
		UniquePtr<Expression> condition;
		UniquePtr<Expression> if_;
		UniquePtr<Expression> then;
	};
	
	
}


#endif // SCATHA_AST_EXPRESSION_H_

