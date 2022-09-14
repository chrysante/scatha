#include "Operator.h"

#include <utl/utility.hpp>

namespace scatha::ast {
	
	std::string_view toString(UnaryPrefixOperator op) {
		using enum UnaryPrefixOperator;
		return UTL_SERIALIZE_ENUM(op, {
			{ Promotion,  "+" },
			{ Negation,   "-" },
			{ BitwiseNot, "~" },
			{ LogicalNot, "!" },
		});
	}
	
	std::string_view toString(BinaryOperator op) {
		using enum BinaryOperator;
		return UTL_SERIALIZE_ENUM(op, {
			{ Multiplication, "*" },
			{ Division,       "/" },
			{ Remainder,      "%" },
			
			{ Addition,       "+" },
			{ Subtraction,    "-" },
			
			{ LeftShift,      "<<" },
			{ RightShift,     ">>" },
			
			{ Less,           "<" },
			{ LessEq,         "<=" },
			{ Greater,        ">" },
			{ GreaterEq,      ">=" },
			
			{ Equals,         "==" },
			{ NotEquals,      "!=" },
			
			{ BitwiseAnd,     "&" },
			{ BitwiseXOr,     "^" },
			{ BitwiseOr,      "|" },
			
			{ LogicalAnd,     "&&" },
			{ LogicalOr,      "||" },
			
			{ Assignment,     "=" },
			{ AddAssignment,  "+=" },
			{ SubAssignment,  "-=" },
			{ MulAssignment,  "*=" },
			{ DivAssignment,  "/=" },
			{ RemAssignment,  "%=" },
			{ LSAssignment,   "<<=" },
			{ RSAssignment,   ">>=" },
			{ AndAssignment,  "&=" },
			{ OrAssignment,   "|=" },
			
			{ Comma,          "," },
		});
	}
	
}
