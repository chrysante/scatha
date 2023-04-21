// SCATHA-PUBLIC-HEADER

#ifndef SCATHA_AST_TOKEN_H_
#define SCATHA_AST_TOKEN_H_

#include <iosfwd>
#include <string>

#include <scatha/AST/Keyword.h>
#include <scatha/AST/SourceLocation.h>
#include <scatha/Common/APFloat.h>
#include <scatha/Common/APInt.h>
#include <scatha/Common/Base.h>

namespace scatha {

enum class TokenType {
    None,
    Identifier,
    IntegerLiteral,
    BooleanLiteral,
    FloatingPointLiteral,
    StringLiteral,
    Punctuation,
    Operator,
    EndOfFile,
    Whitespace,
    Other,
    _count
};

SCATHA_API std::ostream& operator<<(std::ostream&, TokenType);

struct TokenData {
    std::string id;
    TokenType type;
    SourceLocation sourceLocation;

    bool operator==(TokenData const&) const = default;
};

struct SCATHA_API Token: public TokenData {
    Token() = default;
    explicit Token(std::string id,
                   TokenType type,
                   SourceLocation sourceLocation = {}):
        Token({ std::move(id), type, sourceLocation }) {}
    explicit Token(TokenData data): TokenData(std::move(data)) { finalize(); }

    bool empty() const { return id.empty(); }

    APInt toInteger(size_t bitWidth) const;
    APInt toBool() const;
    APFloat toFloat(APFloatPrec precision = APFloatPrec::Double) const;

    bool isSeparator   : 1 = false;
    bool isIdentifier  : 1 = false;
    bool isKeyword     : 1 = false;
    bool isDeclarator  : 1 = false;
    bool isControlFlow : 1 = false;
    bool isPunctuation : 1 = false;

    Keyword keyword{};
    KeywordCategory keywordCategory{};

    bool operator==(Token const& rhs) const {
        return static_cast<TokenData const&>(*this) ==
               static_cast<TokenData const&>(rhs);
    }

private:
    /// Populates all the fields after `id` in token structure.
    void finalize();
};

SCATHA_API std::ostream& operator<<(std::ostream&, Token const&);

} // namespace scatha

#endif // SCATHA_AST_TOKEN_H_
