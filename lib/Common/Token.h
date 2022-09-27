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
		BooleanLiteral,
		FloatingPointLiteral,
		StringLiteral,
		Punctuation,
		Operator,
		EndOfFile,
		Other,
		_count
	};
	
	SCATHA(API) std::ostream& operator<<(std::ostream&, TokenType);
	
	struct SCATHA(API) Token {
		Token() = default;
		explicit Token(std::string id): id(id) {}
		
		bool empty() const { return id.empty(); }
		
		SourceLocation sourceLocation;
		TokenType type;
		std::string id;
		
		u64 toInteger() const;
		bool toBool() const;
		f64 toFloat() const;
	};
	
	SCATHA(API) std::ostream& operator<<(std::ostream&, Token const&);
	
	
	
}

#endif // SCATHA_COMMON_TOKEN_H_
