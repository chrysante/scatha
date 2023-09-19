#include "Parser/LexicalIssue.h"

#include <ostream>

using namespace scatha;
using namespace parser;

void UnexpectedCharacter::format(std::ostream& str) const {
    str << "Unexpected character";
}

void InvalidNumericLiteral::format(std::ostream& str) const {
    str << "Invalid numeric literal";
}

void UnterminatedStringLiteral::format(std::ostream& str) const {
    str << "Unterminated string literal";
}

void UnterminatedCharLiteral::format(std::ostream& str) const {
    str << "Unterminated char literal";
}

void InvalidCharLiteral::format(std::ostream& str) const {
    str << "Invalid char literal";
}

void InvalidEscapeSequence::format(std::ostream& str) const {
    str << "Invalid escape sequence: " << '\\' << litValue;
}

void UnterminatedMultiLineComment::format(std::ostream& str) const {
    str << "Unterminated multiline comment";
}
