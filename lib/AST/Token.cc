#include "AST/Token.h"

#include <array>
#include <ostream>

#include <utl/utility.hpp>

#include "Common/APFloat.h"
#include "Common/APInt.h"

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

APInt Token::toInteger(size_t bitWidth) const {
    SC_ASSERT(type == TokenType::IntegerLiteral,
              "Token is not an integer literal");
    auto value = [&] {
        if (id.size() > 2 &&
            (id.substr(0, 2) == "0x" || id.substr(0, 2) == "0X"))
        {
            return APInt::parse(id.substr(2, id.size() - 2), 16);
        }
        return APInt::parse(id);
    }();
    SC_ASSERT(value, "Invalid literal value");
    SC_ASSERT(value->bitwidth() <= bitWidth,
              "Value too large for integral type");
    value->zext(bitWidth);
    return *value;
}

APInt Token::toBool() const {
    SC_ASSERT(type == TokenType::BooleanLiteral,
              "Token is not an bool literal");
    SC_ASSERT(id == "true" || id == "false", "Must be either true or false");
    return APInt(id == "true" ? 1 : 0, 1);
}

APFloat Token::toFloat(APFloatPrec precision) const {
    SC_ASSERT(type == TokenType::FloatingPointLiteral,
              "Token is not a floating point literal");
    auto const value = APFloat::parse(id, precision);
    SC_ASSERT(value, "Invalid literal value");
    return *value;
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
