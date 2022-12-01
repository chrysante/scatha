#include "Parser/Bracket.h"

#include <utl/utility.hpp>

#include "Common/Token.h"

using namespace scatha;

parse::Bracket parse::toBracket(Token const& token) {
    static constexpr std::string_view brackets[] = {
        "(", ")", "[", "]", "{", "}"
    };
    auto const itr = std::find(std::begin(brackets), std::end(brackets), token.id);
    size_t const index = utl::narrow_cast<size_t>(itr - std::begin(brackets));
    size_t const shiftedIndex = (index + 2) % 8;
    return Bracket{ Bracket::Type(shiftedIndex / 2), Bracket::Side(shiftedIndex % 2) };
}


