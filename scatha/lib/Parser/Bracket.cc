#include "Parser/Bracket.h"

#include <utl/utility.hpp>

#include "Parser/Token.h"

using namespace scatha;
using namespace parser;

Bracket parser::toBracket(Token const& token) {
    using enum Bracket::Type;
    using enum Bracket::Side;
    switch (token.kind()) {
    case TokenKind::OpenParan:
        return { Paranthesis, Open };
    case TokenKind::CloseParan:
        return { Paranthesis, Close };
    case TokenKind::OpenBracket:
        return { Square, Open };
    case TokenKind::CloseBracket:
        return { Square, Close };
    case TokenKind::OpenBrace:
        return { Curly, Open };
    case TokenKind::CloseBrace:
        return { Curly, Close };
    default:
        return { None, Open };
    }
}

std::string parser::toString(Bracket bracket) {
    static std::string const result[3][2] = {
        { "(", ")" },
        { "[", "]" },
        { "{", "}" },
    };
    return result[static_cast<size_t>(bracket.type) - 1]
                 [static_cast<size_t>(bracket.side)];
}

TokenKind parser::toTokenKind(Bracket bracket) {
    if (bracket.side == Bracket::Side::Open) {
        switch (bracket.type) {
        case Bracket::Type::Paranthesis:
            return TokenKind::OpenParan;
        case Bracket::Type::Square:
            return TokenKind::OpenBracket;
        case Bracket::Type::Curly:
            return TokenKind::OpenBrace;
        default:
            SC_UNREACHABLE();
        }
    }
    else {
        switch (bracket.type) {
        case Bracket::Type::Paranthesis:
            return TokenKind::CloseParan;
        case Bracket::Type::Square:
            return TokenKind::CloseBracket;
        case Bracket::Type::Curly:
            return TokenKind::CloseBrace;
        default:
            SC_UNREACHABLE();
        }
    }
}
