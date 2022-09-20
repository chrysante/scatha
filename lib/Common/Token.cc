#include "Common/Token.h"

#include <ostream>
#include <array>

#include <utl/utility.hpp>

namespace scatha {
	
	std::ostream& operator<<(std::ostream& str, TokenType t) {
		using enum TokenType;
	
		return str << UTL_SERIALIZE_ENUM(t, {
			{ Identifier,           "Identifier" },
			{ IntegerLiteral,       "IntegerLiteral" },
			{ FloatingPointLiteral, "FloatingPointLiteral" },
			{ StringLiteral,        "StringLiteral" },
			{ Punctuation,          "Punctuation" },
			{ Operator,             "Operator" },
			{ EndOfFile,            "EndOfFile" },
			{ Other,                "Other" },
		});
	}
	
	std::ostream& operator<<(std::ostream& str, Token const& t) {
		str << "{ ";
		str << t.sourceLocation.line << ", " << t.sourceLocation.column;
		str << ", " << "TokenType::" << t.type;
		str << ", \"" << t.id << "\"";
		str << " }";
		return str;
	}
	
	u64 Token::toInteger() const {
		SC_ASSERT(type == TokenType::IntegerLiteral,
				  "Token is not an integer literal");
		return std::stol(id);
	}
	
}
