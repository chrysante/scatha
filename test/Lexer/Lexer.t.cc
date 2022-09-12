#include <Catch/Catch2.hpp>

#include <iostream>

#include "Lexer/Lexer.h"

using namespace scatha;
using namespace lex;

namespace {

	struct ReferenceToken {
		int line;
		int column;
		TokenType type;
		std::string id;
	};

	struct TestCase {
		std::string text;
		std::vector<ReferenceToken> reference;
	};

	void runTest(int index, TestCase const& test, bool testSC = true) {
		Lexer l(test.text);
		auto const tokens = l.lex();
		
		REQUIRE(tokens.size() == test.reference.size());
		for (std::size_t i = 0; i < tokens.size(); ++i) {
			INFO("Test Index: " << index);
			CHECK(test.reference[i].type == tokens[i].type);
			CHECK(test.reference[i].id == tokens[i].id);
			if (testSC) {
				CHECK(test.reference[i].line == tokens[i].sourceLocation.line);
				CHECK(test.reference[i].column == tokens[i].sourceLocation.column);
			}
		}
	}
	
}

TEST_CASE() {
	return;
 std::string const text = R"(
import std
import myLib

fn main() -> void {
	var text: string = "Hello World!"
	std.print(text)
}
)";
	Lexer l(text);
	auto tokens = l.lex();

	for (auto t: tokens) {
		std::cout << t << ",\n";
	}
}

TEST_CASE() {
	std::vector<ReferenceToken> const aRef = {
		{ 0,   0, TokenType::Punctuation, "EOL" },
		{ 1,   0, TokenType::Identifier,  "fn" },
		{ 1,   3, TokenType::Identifier,  "mul" },
		{ 1,   6, TokenType::Punctuation, "(" },
		{ 1,   7, TokenType::Identifier,  "a" },
		{ 1,   8, TokenType::Punctuation, ":" },
		{ 1,  10, TokenType::Identifier,  "int" },
		{ 1,  13, TokenType::Punctuation, "," },
		{ 1,  15, TokenType::Identifier,  "b" },
		{ 1,  16, TokenType::Punctuation, ":" },
		{ 1,  18, TokenType::Identifier,  "int" },
		{ 1,  21, TokenType::Punctuation, ")" },
		{ 1,  23, TokenType::Operator,    "->" },
		{ 1,  26, TokenType::Identifier,  "int" },
		{ 1,  30, TokenType::Punctuation, "{" },
		{ 1,  31, TokenType::Punctuation, "EOL" },
		{ 2,   1, TokenType::Identifier,  "var" },
		{ 2,   5, TokenType::Identifier,  "result" },
		{ 2,  11, TokenType::Punctuation, ":" },
		{ 2,  13, TokenType::Identifier,  "int" },
		{ 2,  17, TokenType::Operator,    "=" },
		{ 2,  19, TokenType::Identifier,  "a" },
		{ 2,  20, TokenType::Punctuation, "EOL" },
		{ 3,   1, TokenType::Identifier,  "result" },
		{ 3,   8, TokenType::Operator,    "*=" },
		{ 3,  11, TokenType::Identifier,  "b" },
		{ 3,  12, TokenType::Punctuation, ";" },
		{ 3,  14, TokenType::Identifier,  "return" },
		{ 3,  21, TokenType::Identifier,  "result" },
		{ 3,  27, TokenType::Punctuation, "EOL" },
		{ 4,   0, TokenType::Punctuation, "}" },
		{ 4,   1, TokenType::Punctuation, "EOL" },
		{ 5,   0, TokenType::EndOfFile,   "" }
	};
	runTest(0, TestCase{
R"(
fn mul(a: int, b: int) -> int {
	var result: int = a
	result *= b; return result
}
)", aRef });
	runTest(0, TestCase{
R"(
fn mul(a:int,b:int)->int{
	var result:int=a
	result*=b;return result
}
)", aRef }, false);
	
	std::vector<ReferenceToken> const bRef = {
		{ 0, 0, TokenType::Punctuation, "EOL" },
		{ 1, 0, TokenType::Identifier, "import" },
		{ 1, 7, TokenType::Identifier, "std" },
		{ 1, 10, TokenType::Punctuation, "EOL" },
		{ 2, 0, TokenType::Identifier, "import" },
		{ 2, 7, TokenType::Identifier, "myLib" },
		{ 2, 12, TokenType::Punctuation, "EOL" },
		{ 3, 0, TokenType::Punctuation, "EOL" },
		{ 4, 0, TokenType::Identifier, "fn" },
		{ 4, 3, TokenType::Identifier, "main" },
		{ 4, 7, TokenType::Punctuation, "(" },
		{ 4, 8, TokenType::Punctuation, ")" },
		{ 4, 10, TokenType::Punctuation, "{" },
		{ 4, 11, TokenType::Punctuation, "EOL" },
		{ 5, 1, TokenType::Identifier, "var" },
		{ 5, 5, TokenType::Identifier, "text" },
		{ 5, 9, TokenType::Punctuation, ":" },
		{ 5, 11, TokenType::Identifier, "string" },
		{ 5, 18, TokenType::Operator, "=" },
		{ 5, 20, TokenType::Identifier, "generateText" },
		{ 5, 32, TokenType::Punctuation, "(" },
		{ 5, 33, TokenType::Punctuation, ")" },
		{ 5, 34, TokenType::Punctuation, "EOL" },
		{ 6, 1, TokenType::Identifier, "std" },
		{ 6, 4, TokenType::Operator, "." },
		{ 6, 5, TokenType::Identifier, "print" },
		{ 6, 10, TokenType::Punctuation, "(" },
		{ 6, 11, TokenType::Identifier, "text" },
		{ 6, 15, TokenType::Punctuation, ")" },
		{ 6, 16, TokenType::Punctuation, "EOL" },
		{ 7, 0, TokenType::Punctuation, "}" },
		{ 7, 1, TokenType::Punctuation, "EOL" },
		{ 8, 0, TokenType::EndOfFile, "" }
	};
	runTest(1, TestCase{
R"(
import std
import myLib

fn main() {
	var text: string = generateText()
	std.print(text)
}
)", bRef });

	std::vector<ReferenceToken> const cRef = {
		{ 0, 0, TokenType::Punctuation, "EOL" },
		{ 1, 0, TokenType::Identifier, "a" },
		{ 1, 1, TokenType::Operator, "*=" },
		{ 1, 3, TokenType::Identifier, "b" },
		{ 1, 4, TokenType::Punctuation, ";" },
		{ 1, 5, TokenType::Identifier, "x" },
		{ 1, 6, TokenType::Operator, "+=" },
		{ 1, 8, TokenType::NumericLiteral, "1" },
		{ 1, 9, TokenType::Punctuation, ";" },
		{ 1, 10, TokenType::Identifier, "fn" },
		{ 1, 12, TokenType::Punctuation, "(" },
		{ 1, 13, TokenType::Identifier, "true" },
		{ 1, 17, TokenType::Operator, "&&" },
		{ 1, 19, TokenType::Identifier, "false" },
		{ 1, 24, TokenType::Punctuation, ")" },
		{ 1, 25, TokenType::Operator, "+=" },
		{ 1, 27, TokenType::Identifier, "NULL" },
		{ 1, 31, TokenType::Punctuation, ";" },
		{ 1, 32, TokenType::Punctuation, "EOL" },
		{ 2, 0, TokenType::Identifier, "while" },
		{ 2, 6, TokenType::Punctuation, "(" },
		{ 2, 7, TokenType::Identifier, "x" },
		{ 2, 9, TokenType::Operator, ">=" },
		{ 2, 12, TokenType::NumericLiteral, "0" },
		{ 2, 13, TokenType::Punctuation, ")" },
		{ 2, 15, TokenType::Punctuation, "{" },
		{ 2, 16, TokenType::Punctuation, "EOL" },
		{ 3, 1, TokenType::Identifier, "x" },
		{ 3, 3, TokenType::Operator, "-=" },
		{ 3, 6, TokenType::Identifier, "x" },
		{ 3, 8, TokenType::Operator, "%" },
		{ 3, 10, TokenType::NumericLiteral, "3" },
		{ 3, 12, TokenType::Operator, "?" },
		{ 3, 14, TokenType::NumericLiteral, "1" },
		{ 3, 16, TokenType::Punctuation, ":" },
		{ 3, 18, TokenType::NumericLiteral, "2" },
		{ 3, 19, TokenType::Punctuation, ";" },
		{ 3, 20, TokenType::Punctuation, "EOL" },
		{ 4, 0, TokenType::Punctuation, "}" },
		{ 4, 1, TokenType::Punctuation, "EOL" },
		{ 5, 0, TokenType::EndOfFile, "" }
	};
	runTest(2, TestCase{
R"(
a*=b;x+=1;fn(true&&false)+=NULL;
while (x >= 0) {
	x -= x % 3 ? 1 : 2;
}
)", cRef });
	
	std::vector<ReferenceToken> const dRef = {
		{ 0, 0, TokenType::Punctuation, "EOL" },
		{ 1, 0, TokenType::Identifier, "import" },
		{ 1, 7, TokenType::Identifier, "std" },
		{ 1, 10, TokenType::Punctuation, "EOL" },
		{ 2, 0, TokenType::Identifier, "import" },
		{ 2, 7, TokenType::Identifier, "myLib" },
		{ 2, 12, TokenType::Punctuation, "EOL" },
		{ 3, 0, TokenType::Punctuation, "EOL" },
		{ 4, 0, TokenType::Identifier, "fn" },
		{ 4, 3, TokenType::Identifier, "main" },
		{ 4, 7, TokenType::Punctuation, "(" },
		{ 4, 8, TokenType::Punctuation, ")" },
		{ 4, 10, TokenType::Operator, "->" },
		{ 4, 13, TokenType::Identifier, "void" },
		{ 4, 18, TokenType::Punctuation, "{" },
		{ 4, 19, TokenType::Punctuation, "EOL" },
		{ 5, 1, TokenType::Identifier, "var" },
		{ 5, 5, TokenType::Identifier, "text" },
		{ 5, 9, TokenType::Punctuation, ":" },
		{ 5, 11, TokenType::Identifier, "string" },
		{ 5, 18, TokenType::Operator, "=" },
		{ 5, 20, TokenType::StringLiteral, "Hello World!" },
		{ 17, 21, TokenType::Punctuation, "EOL" },
		{ 18, 1, TokenType::Identifier, "std" },
		{ 18, 4, TokenType::Operator, "." },
		{ 18, 5, TokenType::Identifier, "print" },
		{ 18, 10, TokenType::Punctuation, "(" },
		{ 18, 11, TokenType::Identifier, "text" },
		{ 18, 15, TokenType::Punctuation, ")" },
		{ 18, 16, TokenType::Punctuation, "EOL" },
		{ 19, 0, TokenType::Punctuation, "}" },
		{ 19, 1, TokenType::Punctuation, "EOL" },
		{ 20, 0, TokenType::EndOfFile, "" }
	};
	
	runTest(3, TestCase{
R"(
import std
import myLib

fn main() -> void {
	var text: string = "Hello World!"
	std.print(text)
}
)", dRef });
}
