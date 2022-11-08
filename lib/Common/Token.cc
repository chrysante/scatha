#include "Common/Token.h"

#include <array>
#include <ostream>

#include <utl/utility.hpp>

namespace scatha {

std::ostream& operator<<(std::ostream& str, TokenType t) {
    // clang-format off
    return str << UTL_SERIALIZE_ENUM(t, {
        { TokenType::Identifier,           "Identifier" },
        { TokenType::IntegerLiteral,       "IntegerLiteral" },
        { TokenType::BooleanLiteral,       "BooleanLiteral" },
        { TokenType::FloatingPointLiteral, "FloatingPointLiteral" },
        { TokenType::StringLiteral,        "StringLiteral" },
        { TokenType::Punctuation,          "Punctuation" },
        { TokenType::Operator,             "Operator" },
        { TokenType::EndOfFile,            "EndOfFile" },
        { TokenType::Whitespace,           "Whitespace" },
        { TokenType::Other,                "Other" },
    });
    // clang-format on
}

std::ostream& operator<<(std::ostream& str, Token const& t) {
    str << "{ ";
    str << t.sourceLocation.line << ", " << t.sourceLocation.column;
    str << ", "
        << "TokenType::" << t.type;
    str << ", \"" << t.id << "\"";
    str << " }";
    return str;
}

u64 Token::toInteger() const {
    SC_ASSERT(type == TokenType::IntegerLiteral, "Token is not an integer literal");
    if constexpr (sizeof(long) == 8) {
        return std::stoul(id, nullptr, 0);
    }
    else {
        // Unlike on unix based systems, long is 4 bit on 64 bit Windows, so we
        // use long long here.
        static_assert(sizeof(long long) == 8);
        return std::stoull(id, nullptr, 0);
    }
}

bool Token::toBool() const {
    SC_ASSERT(type == TokenType::BooleanLiteral, "Token is not an bool literal");
    SC_ASSERT(id == "true" || id == "false", "Must be either true or false");
    return id == "true" ? true : false;
}

f64 Token::toFloat() const {
    SC_ASSERT(type == TokenType::FloatingPointLiteral, "Token is not a floating point literal");
    static_assert(sizeof(double) == 8);
    return std::stod(id);
}

void finalize(Token& token) {
    if (token.type == TokenType::Punctuation) {
        token.isPunctuation = true;
        if (token.id == ";") {
            token.isSeparator = true;
        }
    }
    if (token.type == TokenType::EndOfFile) {
        token.isPunctuation = true;
        token.isSeparator   = true;
    }

    if (std::optional<Keyword> const keyword = toKeyword(token.id)) {
        token.isKeyword       = true;
        token.keyword         = *keyword;
        token.keywordCategory = categorize(*keyword);
        token.isDeclarator    = isDeclarator(*keyword);
        token.isControlFlow   = isControlFlow(*keyword);
    }

    if (token.type == TokenType::Identifier) {
        token.isIdentifier = true;
    }
}

} // namespace scatha
