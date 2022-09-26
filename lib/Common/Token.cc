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
			{ BooleanLiteral,       "BooleanLiteral" },
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
	
	bool Token::toBool() const {
		SC_ASSERT(type == TokenType::BooleanLiteral,
				  "Token is not an bool literal");
		SC_ASSERT(id == "true" || id == "false", "Must be either true or false");
		return id == "true" ? true : false;
	}
	
	f64 Token::toFloat() const {
		SC_ASSERT(type == TokenType::FloatingPointLiteral,
				  "Token is not a floating point literal");
		return std::stod(id);
	}
	
}
