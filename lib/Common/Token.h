// SCATHA-PUBLIC-HEADER

#ifndef SCATHA_COMMON_TOKEN_H_
#define SCATHA_COMMON_TOKEN_H_

#include <iosfwd>
#include <string>

#include <scatha/Basic/Basic.h>
#include <scatha/Common/APFloat.h>
#include <scatha/Common/APInt.h>
#include <scatha/Common/Keyword.h>
#include <scatha/Common/SourceLocation.h>

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

enum class IdentifierCategory : u8 { Type, Variable, Function };

SCATHA(API) std::ostream& operator<<(std::ostream&, TokenType);

struct TokenData {
    std::string id;
    TokenType type;
    SourceLocation sourceLocation;

    bool operator==(TokenData const&) const = default;
};

struct SCATHA(API) Token: public TokenData {
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

    // Keyword related fields
    Keyword keyword{};
    KeywordCategory keywordCategory{};

    // Identifier related fields
    IdentifierCategory identifierCategory{};

    bool operator==(Token const& rhs) const {
        return static_cast<TokenData const&>(*this) ==
               static_cast<TokenData const&>(rhs);
    }

private:
    /// Populates all the fields after `id` in token structure.
    void finalize();
};

SCATHA(API) std::ostream& operator<<(std::ostream&, Token const&);

} // namespace scatha

#endif // SCATHA_COMMON_TOKEN_H_
