#ifndef SCATHA_AST_OPERATOR_H_
#define SCATHA_AST_OPERATOR_H_

#include <iosfwd>
#include <string_view>

namespace scatha::ast {
	
	enum class UnaryPrefixOperator {
		Promotion,
		Negation,
		BitwiseNot,
		LogicalNot,
		
		_count
	};

	std::string_view toString(UnaryPrefixOperator);
	
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

#endif // SCATHA_AST_OPERATOR_H_

