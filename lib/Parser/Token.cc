#include "Parser/Token.h"

#include <array>
#include <ostream>

#include <utl/utility.hpp>

#include "Common/APFloat.h"
#include "Common/APInt.h"

using namespace scatha;
using namespace parse;

bool parse::isIdentifier(TokenKind kind) {
    switch (kind) {
    case TokenKind::Void:
        [[fallthrough]];
    case TokenKind::Byte:
        [[fallthrough]];
    case TokenKind::Bool:
        [[fallthrough]];
    case TokenKind::Signed8:
        [[fallthrough]];
    case TokenKind::Signed16:
        [[fallthrough]];
    case TokenKind::Signed32:
        [[fallthrough]];
    case TokenKind::Signed64:
        [[fallthrough]];
    case TokenKind::Unsigned8:
        [[fallthrough]];
    case TokenKind::Unsigned16:
        [[fallthrough]];
    case TokenKind::Unsigned32:
        [[fallthrough]];
    case TokenKind::Unsigned64:
        [[fallthrough]];
    case TokenKind::Float32:
        [[fallthrough]];
    case TokenKind::Float64:
        [[fallthrough]];
    case TokenKind::Int:
        [[fallthrough]];
    case TokenKind::Float:
        [[fallthrough]];
    case TokenKind::Identifier:
        return true;
    default:
        return false;
    }
}

APInt Token::toInteger(size_t bitWidth) const {
    SC_ASSERT(kind() == TokenKind::IntegerLiteral,
              "Token is not an integer literal");
    auto value = [&] {
        if (_id.size() > 2 &&
            (_id.substr(0, 2) == "0x" || _id.substr(0, 2) == "0X"))
        {
            return APInt::parse(_id.substr(2, _id.size() - 2), 16);
        }
        return APInt::parse(_id);
    }();
    SC_ASSERT(value, "Invalid literal value");
    SC_ASSERT(value->bitwidth() <= bitWidth,
              "Value too large for integral type");
    value->zext(bitWidth);
    return *value;
}

APInt Token::toBool() const {
    if (kind() == TokenKind::True) {
        return APInt(1, 1);
    }
    else if (kind() == TokenKind::False) {
        return APInt(0, 1);
    }
    SC_UNREACHABLE();
}

APFloat Token::toFloat(APFloatPrec precision) const {
    SC_ASSERT(kind() == TokenKind::FloatLiteral,
              "Token is not a floating point literal");
    auto const value = APFloat::parse(_id, precision);
    SC_ASSERT(value, "Invalid literal value");
    return *value;
}
