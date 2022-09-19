#include <Catch/Catch2.hpp>

#include "Lexer/LexerUtil.h"

using namespace scatha::lex;

TEST_CASE("isLetter", "[lex]") {
	CHECK( isLetter('a'));
	CHECK( isLetter('t'));
	CHECK( isLetter('z'));
	CHECK( isLetter('A'));
	CHECK( isLetter('W'));
	CHECK( isLetter('Z'));
	CHECK( isLetter('_'));
	
	CHECK(!isLetter('0'));
	CHECK(!isLetter('9'));
	CHECK(!isLetter(0));
	
	CHECK( isLetterEx('0'));
	CHECK( isLetterEx('9'));
	CHECK(!isLetterEx(0));
}

TEST_CASE("isDigit", "[lex]") {
	CHECK( isDigitDec('0'));
	CHECK( isDigitDec('9'));
	CHECK(!isDigitDec(0));
	CHECK(!isDigitDec('A'));
	CHECK(!isDigitDec('g'));
	CHECK(!isDigitDec('_'));
}

