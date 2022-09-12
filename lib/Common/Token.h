#pragma once

#ifndef SCATHA_COMMON_TOKEN_H_
#define SCATHA_COMMON_TOKEN_H_

#include <string>
#include <iosfwd>

#include "Basic/Basic.h"
#include "Common/SourceLocation.h"

namespace scatha {
	
	enum class TokenType {
		Identifier,
		NumericLiteral,
		StringLiteral,
		Punctuation,
		Operator,
		EndOfFile,
		_count
	};
	
	std::ostream& operator<<(std::ostream&, TokenType);
	
	struct Token {
		SourceLocation sourceLocation;
		TokenType type;
		std::string id;
	};
	
	std::ostream& operator<<(std::ostream&, Token const&);
	
}

#endif // SCATHA_COMMON_TOKEN_H_
