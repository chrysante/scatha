#pragma once

#ifndef SCATHA_LEXER_LEXERUTIL_H_
#define SCATHA_LEXER_LEXERUTIL_H_

#include <cctype>
#include <utl/common.hpp>

#include "Basic/Basic.h"

namespace scatha::lex {
	
	inline bool isLetter(char c) {
		return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
	}
	
	inline bool isDigitDec(char c) {
		return c >= '0' && c <= '9';
	}
	
	inline bool isLetterEx(char c) {
		return isLetter(c) || isDigitDec(c);
	}
	
	SCATHA(PURE) bool isPunctuation(char);
	
	inline bool isNewline(char c) {
		return c == '\n';
	}
	
	inline bool isSpace(char c) {
		return std::isspace(c) || isNewline(c);
	}
	
	inline bool isDelimiter(char c) {
		return isPunctuation(c) || isSpace(c);
	}
	
	SCATHA(PURE) bool isOperatorLetter(char);
	
	SCATHA(PURE) bool isOperator(std::string_view);
	
}

#endif // SCATHA_LEXER_LEXERUTIL_H_
