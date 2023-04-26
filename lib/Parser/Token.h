// SCATHA-PUBLIC-HEADER

#ifndef SCATHA_AST_TOKEN_H_
#define SCATHA_AST_TOKEN_H_

#include <iosfwd>
#include <string>

#include <scatha/AST/SourceLocation.h>
#include <scatha/Common/APFloat.h>
#include <scatha/Common/APInt.h>
#include <scatha/Common/Base.h>

namespace scatha::parse {

enum class TokenKind {
#define SC_KEYWORD_TOKEN_DEF(Token, _)     Token,
#define SC_OPERATOR_TOKEN_DEF(Token, _)    Token,
#define SC_PUNCTUATION_TOKEN_DEF(Token, _) Token,
#define SC_OTHER_TOKEN_DEF(Token, _)       Token,
#include <scatha/Parser/Token.def>
    _count
};

inline bool isDeclarator(TokenKind kind) {
    return static_cast<int>(kind) >= static_cast<int>(TokenKind::Module) &&
           static_cast<int>(kind) <= static_cast<int>(TokenKind::Let);
}

inline bool isAccessSpec(TokenKind kind) {
    return static_cast<int>(kind) >= static_cast<int>(TokenKind::Public) &&
           static_cast<int>(kind) <= static_cast<int>(TokenKind::Private);
}

bool isIdentifier(TokenKind kind);

struct SCATHA_API Token {
    Token() = default;
    explicit Token(std::string id, TokenKind kind, SourceRange sourceRange):
        _id(std::move(id)), _kind(kind), _sourceRange(sourceRange) {}

    bool empty() const { return _id.empty(); }

    std::string const& id() const { return _id; }

    TokenKind kind() const { return _kind; }

    SourceRange sourceRange() const { return _sourceRange; }

    SourceLocation sourceLocation() const { return _sourceRange.begin(); }

    APInt toInteger(size_t bitWidth) const;

    APInt toBool() const;

    APFloat toFloat(APFloatPrec precision = APFloatPrec::Double) const;

    bool operator==(Token const& rhs) const = default;

private:
    std::string _id;
    TokenKind _kind;
    SourceRange _sourceRange;
};

} // namespace scatha::parse

#endif // SCATHA_AST_TOKEN_H_
