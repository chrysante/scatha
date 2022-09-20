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
		IntegerLiteral,
		FloatingPointLiteral,
		StringLiteral,
		Punctuation,
		Operator,
		EndOfFile,
		Other,
		_count
	};
	
	std::ostream& operator<<(std::ostream&, TokenType);
	
	struct Token {
		Token() = default;
		explicit Token(std::string id): id(id) {}
		
		bool empty() const { return id.empty(); }
		
		SourceLocation sourceLocation;
		TokenType type;
		std::string id;
		
		u64 toInteger() const;
		f64 toFloat() const;
	};
	
	std::ostream& operator<<(std::ostream&, Token const&);
	
	
	
}

#endif // SCATHA_COMMON_TOKEN_H_
