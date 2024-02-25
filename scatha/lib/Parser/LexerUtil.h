#ifndef SCATHA_LEXER_LEXERUTIL_H_
#define SCATHA_LEXER_LEXERUTIL_H_

#include <cctype>
#include <utl/common.hpp>

namespace scatha::lex {

inline bool isLetter(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

inline bool isDigitDec(char c) { return c >= '0' && c <= '9'; }

inline bool isDigitHex(char c) {
    return isDigitDec(c) || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
}

inline bool isFloatDigitDec(char c) { return isDigitDec(c) || c == '.'; }

inline bool isLetterEx(char c) { return isLetter(c) || isDigitDec(c); }

bool isPunctuation(char);

inline bool isNewline(char c) { return c == '\n'; }

inline bool isSpace(char c) { return std::isspace(c) || isNewline(c); }

inline bool isDelimiter(char c) { return isPunctuation(c) || isSpace(c); }

bool isOperatorLetter(char);

bool isOperator(std::string_view);

} // namespace scatha::lex

#endif // SCATHA_LEXER_LEXERUTIL_H_
