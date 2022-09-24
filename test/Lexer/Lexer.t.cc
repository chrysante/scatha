#include <Catch/Catch2.hpp>

#include <iostream>

#include "Lexer/Lexer.h"
#include "Lexer/LexicalIssue.h"

using namespace scatha;
using namespace lex;

namespace {

	struct ReferenceToken {
		TokenType type;
		std::string id;
	};
	
	std::ostream& operator<<(std::ostream& str, ReferenceToken const& t) {
		return str << "{ .type = " << t.type << ", .id = " << t.id << " }";
	}
	
	struct TestCase {
		std::string text;
		std::vector<ReferenceToken> reference;
	};

	void runTest(std::string_view name, TestCase const& test) {
		Lexer l(test.text);
		auto const tokens = l.lex();
		
		CHECK(tokens.size() == test.reference.size());
		
		for (std::size_t i = 0; i < std::min(tokens.size(), test.reference.size()); ++i) {
			INFO(name << ":\nLHS: " << test.reference[i] << "\nRHS: " << tokens[i]);
			CHECK(test.reference[i].type == tokens[i].type);
			CHECK(test.reference[i].id == tokens[i].id);
		}
	}
	
	
	
}

TEST_CASE() {
	return;
	
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

TEST_CASE("Lexer positive", "[lex]") {
	
	SECTION("Section 1") {
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
			{ TokenType::EndOfFile,   "" }
		};
		
		runTest("Section 1", test);
	}
	
	SECTION("Section 2") {
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
			{ TokenType::EndOfFile, "" }
		};
		
		runTest("Section 2", test);
	}

	SECTION("Section 3") {
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
			{ TokenType::IntegerLiteral, "1" },
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
			{ TokenType::IntegerLiteral, "0" },
			{ TokenType::Punctuation, ")" },
			{ TokenType::Punctuation, "{" },
			{ TokenType::Identifier, "x" },
			{ TokenType::Operator, "-=" },
			{ TokenType::Identifier, "x" },
			{ TokenType::Operator, "%" },
			{ TokenType::IntegerLiteral, "3" },
			{ TokenType::Operator, "?" },
			{ TokenType::IntegerLiteral, "1" },
			{ TokenType::Punctuation, ":" },
			{ TokenType::IntegerLiteral, "2" },
			{ TokenType::Punctuation, ";" },
			{ TokenType::Punctuation, "}" },
			{ TokenType::EndOfFile, "" }
		};
		runTest("Section 3", test);
	}
	
	SECTION("Section 4") {
		TestCase test;
		test.text = R"(
import std;
import myLib;

fn main() -> void {
/*
an ignored multi line comment
*/
	var text_: string = "Hello World!";
	std.print(_text);
	1.0;
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
			{ TokenType::Identifier, "text_" },
			{ TokenType::Punctuation, ":" },
			{ TokenType::Identifier, "string" },
			{ TokenType::Operator, "=" },
			{ TokenType::StringLiteral, "Hello World!" },
			{ TokenType::Punctuation, ";" },
			{ TokenType::Identifier, "std" },
			{ TokenType::Operator, "." },
			{ TokenType::Identifier, "print" },
			{ TokenType::Punctuation, "(" },
			{ TokenType::Identifier, "_text" },
			{ TokenType::Punctuation, ")" },
			{ TokenType::Punctuation, ";" },
			{ TokenType::FloatingPointLiteral, "1.0" },
			{ TokenType::Punctuation, ";" },
			{ TokenType::Punctuation, "}" },
			{ TokenType::EndOfFile, "" }
		};
		runTest("Section 4", test);
	}
	
}

static auto lexString(std::string_view text) {
	Lexer l(text);
	return l.lex();
}

TEST_CASE("Lexer literals", "[lex]") {
	TestCase test;
	test.text = R"(
39;
x = 39;
f(39);
)";
	test.reference = {
		{ TokenType::IntegerLiteral, "39" },
		{ TokenType::Punctuation, ";" },
		{ TokenType::Identifier, "x" },
		{ TokenType::Operator, "=" },
		{ TokenType::IntegerLiteral, "39" },
		{ TokenType::Punctuation, ";" },
		{ TokenType::Identifier, "f" },
		{ TokenType::Punctuation, "(" },
		{ TokenType::IntegerLiteral, "39" },
		{ TokenType::Punctuation, ")" },
		{ TokenType::Punctuation, ";" },
		{ TokenType::EndOfFile, "" }
	};
	runTest("Section 4", test);
}

TEST_CASE("Lexer negative", "[lex]") {
	CHECK_THROWS_AS(lexString("123someID"), InvalidNumericLiteral);
	CHECK_THROWS_AS(lexString("123.23someID"), InvalidNumericLiteral);
	CHECK_THROWS_AS(lexString(R"("begin string
 and end on next, line"))"), UnterminatedStringLiteral);
}
