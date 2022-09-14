#include <Catch/Catch2.hpp>

#include <iostream>

#include "Lexer/Lexer.h"

using namespace scatha;
using namespace lex;

namespace {

	struct ReferenceToken {
		TokenType type;
		std::string id;
	};

	struct TestCase {
		std::string text;
		std::vector<ReferenceToken> reference;
	};

	void runTest(std::string_view name, TestCase const& test) {
		Lexer l(test.text);
		auto const tokens = l.lex();
		
		INFO(name);
		
		REQUIRE(tokens.size() == test.reference.size());
		for (std::size_t i = 0; i < tokens.size(); ++i) {
			CHECK(test.reference[i].type == tokens[i].type);
			CHECK(test.reference[i].id == tokens[i].id);
		}
	}
	
	
	
}

TEST_CASE() { return;
	
 std::string const text = R"(
import std;
import myLib;

fn main() -> void {
	var text: string = "Hello World!";
	std.print(text);
}
)";
	Lexer l(text);
	auto tokens = l.lex();

	for (auto t: tokens) {
		std::cout << t << ",\n";
	}
}

TEST_CASE("Lexer [no source location]") {
	
	SECTION("1") {
		TestCase test;
		test.text = R"(
 fn mul(a: int, b: int) -> int {
	 var result: int = a;
	 result *= b; return result;
 }
 )";
		test.reference = {
			{ TokenType::Identifier,  "fn" },
			{ TokenType::Identifier,  "mul" },
			{ TokenType::Punctuation, "(" },
			{ TokenType::Identifier,  "a" },
			{ TokenType::Punctuation, ":" },
			{ TokenType::Identifier,  "int" },
			{ TokenType::Punctuation, "," },
			{ TokenType::Identifier,  "b" },
			{ TokenType::Punctuation, ":" },
			{ TokenType::Identifier,  "int" },
			{ TokenType::Punctuation, ")" },
			{ TokenType::Operator,    "->" },
			{ TokenType::Identifier,  "int" },
			{ TokenType::Punctuation, "{" },
			{ TokenType::Identifier,  "var" },
			{ TokenType::Identifier,  "result" },
			{ TokenType::Punctuation, ":" },
			{ TokenType::Identifier,  "int" },
			{ TokenType::Operator,    "=" },
			{ TokenType::Identifier,  "a" },
			{ TokenType::Punctuation, ";" },
			{ TokenType::Identifier,  "result" },
			{ TokenType::Operator,    "*=" },
			{ TokenType::Identifier,  "b" },
			{ TokenType::Punctuation, ";" },
			{ TokenType::Identifier,  "return" },
			{ TokenType::Identifier,  "result" },
			{ TokenType::Punctuation, ";" },
			{ TokenType::Punctuation, "}" },
			{ TokenType::EndOfFile }
		};
		
		runTest("1", test);
	}
	
	SECTION("2") {
		TestCase test;
		test.text = R"(
import std;
import myLib;

fn main() {
	var text: string = generateText();
	std.print(text);
}
)";
		test.reference = {
			{ TokenType::Identifier, "import" },
			{ TokenType::Identifier, "std" },
			{ TokenType::Punctuation, ";" },
			{ TokenType::Identifier, "import" },
			{ TokenType::Identifier, "myLib" },
			{ TokenType::Punctuation, ";" },
			{ TokenType::Identifier, "fn" },
			{ TokenType::Identifier, "main" },
			{ TokenType::Punctuation, "(" },
			{ TokenType::Punctuation, ")" },
			{ TokenType::Punctuation, "{" },
			{ TokenType::Identifier, "var" },
			{ TokenType::Identifier, "text" },
			{ TokenType::Punctuation, ":" },
			{ TokenType::Identifier, "string" },
			{ TokenType::Operator, "=" },
			{ TokenType::Identifier, "generateText" },
			{ TokenType::Punctuation, "(" },
			{ TokenType::Punctuation, ")" },
			{ TokenType::Punctuation, ";" },
			{ TokenType::Identifier, "std" },
			{ TokenType::Operator, "." },
			{ TokenType::Identifier, "print" },
			{ TokenType::Punctuation, "(" },
			{ TokenType::Identifier, "text" },
			{ TokenType::Punctuation, ")" },
			{ TokenType::Punctuation, ";" },
			{ TokenType::Punctuation, "}" },
			{ TokenType::EndOfFile }
		};
		
		runTest("2", test);
	}

	SECTION("3") {
		TestCase test;
		test.text = R"(
a*=b;x+=1;fn(true&&false)+=NULL;
while (x >= 0) {
	x -= x % 3 ? 1 : 2;
}
)";
		test.reference = {
			{ TokenType::Identifier, "a" },
			{ TokenType::Operator, "*=" },
			{ TokenType::Identifier, "b" },
			{ TokenType::Punctuation, ";" },
			{ TokenType::Identifier, "x" },
			{ TokenType::Operator, "+=" },
			{ TokenType::NumericLiteral, "1" },
			{ TokenType::Punctuation, ";" },
			{ TokenType::Identifier, "fn" },
			{ TokenType::Punctuation, "(" },
			{ TokenType::Identifier, "true" },
			{ TokenType::Operator, "&&" },
			{ TokenType::Identifier, "false" },
			{ TokenType::Punctuation, ")" },
			{ TokenType::Operator, "+=" },
			{ TokenType::Identifier, "NULL" },
			{ TokenType::Punctuation, ";" },
			{ TokenType::Identifier, "while" },
			{ TokenType::Punctuation, "(" },
			{ TokenType::Identifier, "x" },
			{ TokenType::Operator, ">=" },
			{ TokenType::NumericLiteral, "0" },
			{ TokenType::Punctuation, ")" },
			{ TokenType::Punctuation, "{" },
			{ TokenType::Identifier, "x" },
			{ TokenType::Operator, "-=" },
			{ TokenType::Identifier, "x" },
			{ TokenType::Operator, "%" },
			{ TokenType::NumericLiteral, "3" },
			{ TokenType::Operator, "?" },
			{ TokenType::NumericLiteral, "1" },
			{ TokenType::Punctuation, ":" },
			{ TokenType::NumericLiteral, "2" },
			{ TokenType::Punctuation, ";" },
			{ TokenType::Punctuation, "}" },
			{ TokenType::EndOfFile }
		};
		runTest("3", test);
	}
	
	SECTION("4") {
		TestCase test;
		test.text = R"(
import std;
import myLib;

fn main() -> void {
	var text: string = "Hello World!";
	std.print(text);
}
)";
		test.reference = {
			{ TokenType::Identifier, "import" },
			{ TokenType::Identifier, "std" },
			{ TokenType::Punctuation, ";" },
			{ TokenType::Identifier, "import" },
			{ TokenType::Identifier, "myLib" },
			{ TokenType::Punctuation, ";" },
			{ TokenType::Identifier, "fn" },
			{ TokenType::Identifier, "main" },
			{ TokenType::Punctuation, "(" },
			{ TokenType::Punctuation, ")" },
			{ TokenType::Operator, "->" },
			{ TokenType::Identifier, "void" },
			{ TokenType::Punctuation, "{" },
			{ TokenType::Identifier, "var" },
			{ TokenType::Identifier, "text" },
			{ TokenType::Punctuation, ":" },
			{ TokenType::Identifier, "string" },
			{ TokenType::Operator, "=" },
			{ TokenType::StringLiteral, "Hello World!" },
			{ TokenType::Punctuation, ";" },
			{ TokenType::Identifier, "std" },
			{ TokenType::Operator, "." },
			{ TokenType::Identifier, "print" },
			{ TokenType::Punctuation, "(" },
			{ TokenType::Identifier, "text" },
			{ TokenType::Punctuation, ")" },
			{ TokenType::Punctuation, ";" },
			{ TokenType::Punctuation, "}" },
			{ TokenType::EndOfFile, "" }
		};
		runTest("4", test);
	}
	
}
