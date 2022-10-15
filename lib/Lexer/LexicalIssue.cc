#include "Lexer/LexicalIssue.h"

namespace scatha::lex {

LexicalIssue::LexicalIssue(Token const& token, std::string const& what):
    ProgramIssue(token, "Lexical Issue: " + what) {}

UnexpectedID::UnexpectedID(Token const& token): LexicalIssue(token, "Unexpected ID") {}

InvalidNumericLiteral::InvalidNumericLiteral(Token const& token): LexicalIssue(token, "Invalid numeric literal") {}

UnterminatedMultiLineComment::UnterminatedMultiLineComment(Token const& token):
    LexicalIssue(token, "Unterminated multi line comment") {}

UnterminatedStringLiteral::UnterminatedStringLiteral(Token const& token):
    LexicalIssue(token, "Unterminated string literal") {}

} // namespace scatha::lex
