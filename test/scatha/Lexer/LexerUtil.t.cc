#include <catch2/catch_test_macros.hpp>

#include "Parser/LexerUtil.h"

using namespace scatha::lex;

TEST_CASE("isLetter", "[lex]") {
    CHECK(isLetter('a'));
    CHECK(isLetter('t'));
    CHECK(isLetter('z'));
    CHECK(isLetter('A'));
    CHECK(isLetter('W'));
    CHECK(isLetter('Z'));
    CHECK(isLetter('_'));
    CHECK(!isLetter('0'));
    CHECK(!isLetter('9'));
    CHECK(!isLetter(0));
    CHECK(isLetterEx('0'));
    CHECK(isLetterEx('9'));
    CHECK(!isLetterEx(0));
}

TEST_CASE("isDigitDec", "[lex]") {
    CHECK(isDigitDec('0'));
    CHECK(isDigitDec('9'));
    CHECK(!isDigitDec(0)); /// Note missing char quotes.
    CHECK(!isDigitDec('A'));
    CHECK(!isDigitDec('g'));
    CHECK(!isDigitDec('_'));
}

TEST_CASE("isDigitHex", "[lex]") {
    CHECK(isDigitHex('0'));
    CHECK(isDigitHex('9'));
    CHECK(!isDigitHex(0)); /// Note missing char quotes.
    CHECK(isDigitHex('A'));
    CHECK(isDigitHex('a'));
    CHECK(isDigitHex('E'));
    CHECK(isDigitHex('e'));
    CHECK(isDigitHex('F'));
    CHECK(isDigitHex('f'));
    CHECK(!isDigitHex('g'));
    CHECK(!isDigitHex('_'));
}
