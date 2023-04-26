#include <Catch/Catch2.hpp>

#include "Parser/Bracket.h"
#include "Parser/Token.h"

using namespace scatha;
using namespace parse;

namespace {

void checkBracketImpl(std::string str,
                      TokenKind tokenType,
                      Bracket::Type type,
                      Bracket::Side side) {
    Token const t(std::move(str), tokenType, SourceRange{});
    Bracket const bracket = toBracket(t);
    CHECK(bracket.type == type);
    if (type != Bracket::Type::None) {
        CHECK(bracket.side == side);
    }
}

void checkBracket(std::string str, Bracket::Type type, Bracket::Side side) {
    checkBracketImpl(std::move(str), TokenKind::_count, type, side);
}

void checkNone(std::string str, TokenKind type) {
    checkBracketImpl(std::move(str), type, Bracket::Type::None, {});
}

} // namespace

TEST_CASE("Bracket None", "[parse][preparse]") {
    checkNone("var", TokenKind::Identifier);
    checkNone("-", TokenKind::Minus);
    checkNone("+", TokenKind::Plus);
    checkNone("123", TokenKind::IntegerLiteral);
    checkNone("?", TokenKind::Question);
    checkNone("_", TokenKind::Identifier);
}

TEST_CASE("Bracket", "[parse][preparse]") {
    using enum Bracket::Type;
    using enum Bracket::Side;
    checkBracket("(", Parenthesis, Open);
    checkBracket(")", Parenthesis, Close);
    checkBracket("[", Square, Open);
    checkBracket("]", Square, Close);
    checkBracket("{", Curly, Open);
    checkBracket("}", Curly, Close);
}
