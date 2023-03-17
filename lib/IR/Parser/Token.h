#ifndef SCATHA_IR_PARSER_TOKEN_H_
#define SCATHA_IR_PARSER_TOKEN_H_

#include <string_view>

#include "Basic/Basic.h"
#include "IR/Parser/SourceLocation.h"

namespace scatha::ir {

enum class TokenKind {
    GlobalIdentifier,
    LocalIdentifier,
    Keyword,
    IntType,
    FloatType,
    IntLiteral,
    Punctuation,
    EndOfFile
};

class Token {
public:
    explicit Token(std::string_view id,
                   SourceLocation loc,
                   TokenKind kind,
                   unsigned width = 0):
        _id(id), _loc(loc), _kind(kind), _width(width) {}
    
    explicit Token(char const* first,
                   char const* last,
                   SourceLocation loc,
                   TokenKind kind,
                   unsigned width = 0):
        Token(std::string_view(first, static_cast<size_t>(last - first)),
              loc,
              kind,
              width) {}

    std::string_view id() const { return _id; }
    
    SourceLocation sourceLocation() const { return _loc; }
    
    TokenKind kind() const { return _kind; }
    
    /// Width of integral or float type. Only applicable if `kind() == IntType`
    /// or `kind() == FloatType`
    unsigned width() const { return _width; }
    
private:
    std::string_view _id;
    SourceLocation _loc;
    TokenKind _kind;
    unsigned _width;
};

} // namespace scatha::ir

#endif // SCATHA_IR_PARSER_TOKEN_H_
