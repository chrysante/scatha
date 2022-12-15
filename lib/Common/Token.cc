#include "Common/Token.h"

#include <array>
#include <ostream>

#include <utl/utility.hpp>

namespace scatha {

std::ostream& operator<<(std::ostream& str, TokenType t) {
    // clang-format off
    return str << UTL_SERIALIZE_ENUM(t, {
        { TokenType::None,                 "None" },
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

std::optional<BigNum> Token::toBigNum() const {
    return BigNum::fromString(id);
}

u64 Token::toInteger() const {
    SC_ASSERT(type == TokenType::IntegerLiteral, "Token is not an integer literal");
    auto const value = toBigNum();
    SC_ASSERT(value, "Invalid literal value");
    return static_cast<u64>(*value);
}

bool Token::toBool() const {
    SC_ASSERT(type == TokenType::BooleanLiteral, "Token is not an bool literal");
    SC_ASSERT(id == "true" || id == "false", "Must be either true or false");
    return id == "true" ? true : false;
}

f64 Token::toFloat() const {
    SC_ASSERT(type == TokenType::FloatingPointLiteral, "Token is not a floating point literal");
    auto const value = toBigNum();
    SC_ASSERT(value, "Invalid literal value");
    return static_cast<f64>(*value);
}

void Token::finalize() {
    if (type == TokenType::Punctuation) {
        isPunctuation = true;
        if (id == ";") {
            isSeparator = true;
        }
    }
    if (type == TokenType::EndOfFile) {
        isPunctuation = true;
        isSeparator   = true;
    }

    if (std::optional<Keyword> const kw = toKeyword(id)) {
        isKeyword       = true;
        keyword         = *kw;
        keywordCategory = categorize(*kw);
        isDeclarator    = scatha::isDeclarator(*kw);
        isControlFlow   = scatha::isControlFlow(*kw);
    }

    if (type == TokenType::Identifier) {
        isIdentifier = true;
    }
}

} // namespace scatha
