#pragma once

#ifndef SCATHA_COMMON_TOKEN_H_
#define SCATHA_COMMON_TOKEN_H_

#include <string>
#include <iosfwd>

#include "Basic/Basic.h"
#include "Common/SourceLocation.h"
#include "Common/Keyword.h"

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
	
	enum class IdentifierCategory: u8 {
		Type, Variable, Function
	};
	
	SCATHA(API) std::ostream& operator<<(std::ostream&, TokenType);
	
	struct SCATHA(API) Token {
		Token() = default;
		explicit Token(std::string id): id(std::move(id)) {}
		
		bool empty() const { return id.empty(); }
		
		u64 toInteger() const;
		bool toBool() const;
		f64 toFloat() const;
		
		SourceLocation sourceLocation;
		TokenType type;
		std::string id;
		
		bool isSeparator   : 1 = false;
		bool isEOL         : 1 = false;
		bool isIdentifier  : 1 = false;
		bool isKeyword     : 1 = false;
		bool isDeclarator  : 1 = false;
		bool isControlFlow : 1 = false;
		bool isPunctuation : 1 = false;
		
		// Keyword related fields
		Keyword keyword{};
		KeywordCategory keywordCategory{};
		
		// Identifier related fields
		IdentifierCategory identifierCategory{};
	};
	
	using TokenEx = Token;
	
	SCATHA(API) std::ostream& operator<<(std::ostream&, Token const&);

	/// Populates all the fields after \p id in token structure.
	void finalize(Token&);
	
}

#endif // SCATHA_COMMON_TOKEN_H_
