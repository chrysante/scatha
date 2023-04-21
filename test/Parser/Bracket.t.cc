#include <Catch/Catch2.hpp>

#include "AST/Token.h"
#include "Parser/Bracket.h"

using namespace scatha;
using namespace parse;

namespace {

void checkBracketImpl(std::string str,
                      TokenType tokenType,
                      Bracket::Type type,
                      Bracket::Side side) {
    Token const t(std::move(str), tokenType);
    Bracket const bracket = toBracket(t);
    CHECK(bracket.type == type);
    if (type != Bracket::Type::None) {
        CHECK(bracket.side == side);
    }
}

void checkBracket(std::string str, Bracket::Type type, Bracket::Side side) {
    checkBracketImpl(std::move(str), TokenType::Punctuation, type, side);
}

void checkNone(std::string str, TokenType type) {
    checkBracketImpl(std::move(str), type, Bracket::Type::None, {});
}

} // namespace

TEST_CASE("Bracket None", "[parse][preparse]") {
    checkNone("var", TokenType::Identifier);
    checkNone("-", TokenType::Operator);
    checkNone("+", TokenType::Operator);
    checkNone("123", TokenType::IntegerLiteral);
    checkNone("?", TokenType::Operator);
    checkNone("_", TokenType::Identifier);
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
