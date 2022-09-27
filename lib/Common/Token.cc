#include "Common/Token.h"

#include <ostream>
#include <array>

#include <utl/utility.hpp>

namespace scatha {
	
	std::ostream& operator<<(std::ostream& str, TokenType t) {
		return str << UTL_SERIALIZE_ENUM(t, {
			{ TokenType::Identifier,           "Identifier" },
			{ TokenType::IntegerLiteral,       "IntegerLiteral" },
			{ TokenType::BooleanLiteral,       "BooleanLiteral" },
			{ TokenType::FloatingPointLiteral, "FloatingPointLiteral" },
			{ TokenType::StringLiteral,        "StringLiteral" },
			{ TokenType::Punctuation,          "Punctuation" },
			{ TokenType::Operator,             "Operator" },
			{ TokenType::EndOfFile,            "EndOfFile" },
			{ TokenType::Other,                "Other" },
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
		if constexpr (sizeof(long) == 8) {
			return std::stol(id, nullptr, 0);
		}
		else {
			// Unlike on unix based systems, long is 4 bit on 64 bit Windows, so we use long long here.
			static_assert(sizeof(long long) == 8);
			return std::stoll(id, nullptr, 0);
		}
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
		static_assert(sizeof(double) == 8);
		return std::stod(id);
	}
	
}
