/// This header is public because syntax issues contain tokens

#ifndef SCATHA_PARSER_TOKEN_H_
#define SCATHA_PARSER_TOKEN_H_

#include <string>

#include <scatha/Common/APMathFwd.h>
#include <scatha/Common/Base.h>
#include <scatha/Common/SourceLocation.h>

namespace scatha::parser {

enum class TokenKind {
#define SC_KEYWORD_TOKEN_DEF(Token, _)     Token,
#define SC_OPERATOR_TOKEN_DEF(Token, _)    Token,
#define SC_PUNCTUATION_TOKEN_DEF(Token, _) Token,
#define SC_OTHER_TOKEN_DEF(Token, _)       Token,
#include <scatha/Parser/Token.def.h>
};

/// \Returns the case name of \p tokenKind as a string
SCATHA_API std::string toString(TokenKind tokenKind);

inline bool isDeclarator(TokenKind kind) {
    return static_cast<int>(kind) >= static_cast<int>(TokenKind::Module) &&
           static_cast<int>(kind) <= static_cast<int>(TokenKind::Let);
}

inline bool isAccessSpec(TokenKind kind) {
    return static_cast<int>(kind) >= static_cast<int>(TokenKind::Public) &&
           static_cast<int>(kind) <= static_cast<int>(TokenKind::Private);
}

/// \Returns `true` if this token is an identifier or a the name of a builtin
/// type
bool isID(TokenKind kind);

/// \Returns `true` if this token is an identifier or a keyword that may denote
/// a name in certain contexts like `move`
bool isExtendedID(TokenKind kind);

///
///
///
struct SCATHA_API Token {
    ///
    Token() = default;

    ///
    explicit Token(std::string id, TokenKind kind, SourceRange sourceRange):
        _id(std::move(id)), _kind(kind), _sourceRange(sourceRange) {}

    ///
    bool empty() const { return _id.empty(); }

    ///
    std::string const& id() const { return _id; }

    ///
    TokenKind kind() const { return _kind; }

    ///
    SourceRange sourceRange() const { return _sourceRange; }

    ///
    SourceLocation sourceLocation() const { return _sourceRange.begin(); }

    ///
    APInt toInteger(size_t bitwidth) const;

    ///
    APInt toBool() const;

    /// \Returns `toFloat(APFloatPrec::Double())`
    APFloat toFloat() const;

    ///
    APFloat toFloat(APFloatPrec precision) const;

    ///
    bool operator==(Token const& rhs) const = default;

private:
    std::string _id;
    TokenKind _kind;
    SourceRange _sourceRange;
};

} // namespace scatha::parser

#endif // SCATHA_PARSER_TOKEN_H_
