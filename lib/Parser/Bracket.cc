#include "Parser/Bracket.h"

#include <utl/utility.hpp>

#include "Parser/Token.h"

using namespace scatha;
using namespace parse;

static constexpr std::string_view brackets[] = { "(", ")", "[", "]", "{", "}" };

parse::Bracket parse::toBracket(Token const& token) {
    auto const itr =
        std::find(std::begin(brackets), std::end(brackets), token.id());
    size_t const index = utl::narrow_cast<size_t>(itr - std::begin(brackets));
    size_t const shiftedIndex = (index + 2) % 8;
    return Bracket{ Bracket::Type(shiftedIndex / 2),
                    Bracket::Side(shiftedIndex % 2) };
}

std::string parse::toString(Bracket bracket) {
    size_t const index = (static_cast<size_t>(bracket.type) - 1) * 2 +
                         static_cast<size_t>(bracket.side);
    SC_ASSERT(index < std::size(brackets), "Out of bounds");
    return std::string(brackets[index]);
}

TokenKind parse::toTokenKind(Bracket bracket) {
    if (bracket.side == Bracket::Side::Open) {
        switch (bracket.type) {
        case Bracket::Type::Parenthesis:
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
        case Bracket::Type::Parenthesis:
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
