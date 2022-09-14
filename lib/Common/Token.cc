#include "Common/Token.h"

#include <ostream>
#include <array>

#include <utl/utility.hpp>

namespace scatha {
	
	std::ostream& operator<<(std::ostream& str, TokenType t) {
		using enum TokenType;
	
		return str << UTL_SERIALIZE_ENUM(t, {
			{ Identifier,     "Identifier" },
			{ NumericLiteral, "NumericLiteral" },
			{ StringLiteral,  "StringLiteral" },
			{ Punctuation,    "Punctuation" },
			{ Operator,       "Operator" },
			{ EndOfFile,      "EndOfFile" }
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
	
	
}
