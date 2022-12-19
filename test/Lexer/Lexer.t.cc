#include <Catch/Catch2.hpp>

#include <iostream>

#include "Lexer/Lexer.h"
#include "Lexer/LexicalIssue.h"
#include "test/IssueHelper.h"

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
    void run() const {
        issue::LexicalIssueHandler iss;
        auto const tokens = lex::lex(text, iss);
        REQUIRE(iss.empty());
        REQUIRE(tokens.size() == reference.size());
        for (std::size_t i = 0; i < std::min(tokens.size(), reference.size()); ++i) {
            INFO("LHS: " << reference[i] << "\nRHS: " << tokens[i]);
            CHECK(reference[i].type == tokens[i].type);
            CHECK(reference[i].id == tokens[i].id);
        }
    }
};

} // namespace

TEST_CASE("Lexer positive 1", "[lex]") {
    TestCase test;
    test.text      = R"(
fn mul(a: int, b: int) -> int {
	var result: int = a;
	result *= b; return result;
}
)";
    test.reference = {
        { TokenType::Identifier, "fn" },     { TokenType::Identifier, "mul" },    { TokenType::Punctuation, "(" },
        { TokenType::Identifier, "a" },      { TokenType::Punctuation, ":" },     { TokenType::Identifier, "int" },
        { TokenType::Punctuation, "," },     { TokenType::Identifier, "b" },      { TokenType::Punctuation, ":" },
        { TokenType::Identifier, "int" },    { TokenType::Punctuation, ")" },     { TokenType::Operator, "->" },
        { TokenType::Identifier, "int" },    { TokenType::Punctuation, "{" },     { TokenType::Identifier, "var" },
        { TokenType::Identifier, "result" }, { TokenType::Punctuation, ":" },     { TokenType::Identifier, "int" },
        { TokenType::Operator, "=" },        { TokenType::Identifier, "a" },      { TokenType::Punctuation, ";" },
        { TokenType::Identifier, "result" }, { TokenType::Operator, "*=" },       { TokenType::Identifier, "b" },
        { TokenType::Punctuation, ";" },     { TokenType::Identifier, "return" }, { TokenType::Identifier, "result" },
        { TokenType::Punctuation, ";" },     { TokenType::Punctuation, "}" },     { TokenType::EndOfFile, "" }
    };
    test.run();
}

TEST_CASE("Lexer positive 2", "[lex]") {
    TestCase test;
    test.text      = R"(
import std;
import myLib;

fn main() {
	var text: string = f();
	std.print(text);
}
)";
    test.reference = {
        { TokenType::Identifier, "import" }, { TokenType::Identifier, "std" },   { TokenType::Punctuation, ";" },
        { TokenType::Identifier, "import" }, { TokenType::Identifier, "myLib" }, { TokenType::Punctuation, ";" },
        { TokenType::Identifier, "fn" },     { TokenType::Identifier, "main" },  { TokenType::Punctuation, "(" },
        { TokenType::Punctuation, ")" },     { TokenType::Punctuation, "{" },    { TokenType::Identifier, "var" },
        { TokenType::Identifier, "text" },   { TokenType::Punctuation, ":" },    { TokenType::Identifier, "string" },
        { TokenType::Operator, "=" },        { TokenType::Identifier, "f" },     { TokenType::Punctuation, "(" },
        { TokenType::Punctuation, ")" },     { TokenType::Punctuation, ";" },    { TokenType::Identifier, "std" },
        { TokenType::Operator, "." },        { TokenType::Identifier, "print" }, { TokenType::Punctuation, "(" },
        { TokenType::Identifier, "text" },   { TokenType::Punctuation, ")" },    { TokenType::Punctuation, ";" },
        { TokenType::Punctuation, "}" },     { TokenType::EndOfFile, "" }
    };
    test.run();
}

TEST_CASE("Lexer positive 3", "[lex]") {
    TestCase test;
    test.text      = R"(
a*=b;x+=1;fn(true&&false)+=NULL;
while (x >= 0) {
	x -= x % 3  ? 1 : 2;
}
)";
    test.reference = { { TokenType::Identifier, "a" },         { TokenType::Operator, "*=" },
                       { TokenType::Identifier, "b" },         { TokenType::Punctuation, ";" },
                       { TokenType::Identifier, "x" },         { TokenType::Operator, "+=" },
                       { TokenType::IntegerLiteral, "1" },     { TokenType::Punctuation, ";" },
                       { TokenType::Identifier, "fn" },        { TokenType::Punctuation, "(" },
                       { TokenType::BooleanLiteral, "true" },  { TokenType::Operator, "&&" },
                       { TokenType::BooleanLiteral, "false" }, { TokenType::Punctuation, ")" },
                       { TokenType::Operator, "+=" },          { TokenType::Identifier, "NULL" },
                       { TokenType::Punctuation, ";" },        { TokenType::Identifier, "while" },
                       { TokenType::Punctuation, "(" },        { TokenType::Identifier, "x" },
                       { TokenType::Operator, ">=" },          { TokenType::IntegerLiteral, "0" },
                       { TokenType::Punctuation, ")" },        { TokenType::Punctuation, "{" },
                       { TokenType::Identifier, "x" },         { TokenType::Operator, "-=" },
                       { TokenType::Identifier, "x" },         { TokenType::Operator, "%" },
                       { TokenType::IntegerLiteral, "3" },     { TokenType::Operator, "?" },
                       { TokenType::IntegerLiteral, "1" },     { TokenType::Punctuation, ":" },
                       { TokenType::IntegerLiteral, "2" },     { TokenType::Punctuation, ";" },
                       { TokenType::Punctuation, "}" },        { TokenType::EndOfFile, "" } };
    test.run();
}

TEST_CASE("Lexer positive 4", "[lex]") {
    TestCase test;
    test.text      = R"(
import std;
import myLib;

fn main() -> void {
/*
an ignored multi line comment
*/
	var text_: string = "Hello World!";
	""
	std.print(_text);
	1.0;
}
)";
    test.reference = { { TokenType::Identifier, "import" },
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
                       { TokenType::StringLiteral, "" },
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
                       { TokenType::EndOfFile, "" } };
    test.run();
}

TEST_CASE("Lexer literals", "[lex]") {
    return;
    TestCase test;
    test.text      = R"(
0.0
39;
x = 39;
0x39;
x = 0x39;
0x39e;
x = 0x39e;
f(39);
f(39.0);
f(true);
false;
true
)";
    test.reference = { { TokenType::FloatingPointLiteral, "0.0" },
                       { TokenType::IntegerLiteral, "39" },
                       { TokenType::Punctuation, ";" },
                       { TokenType::Identifier, "x" },
                       { TokenType::Operator, "=" },
                       { TokenType::IntegerLiteral, "39" },
                       { TokenType::Punctuation, ";" },
                       { TokenType::IntegerLiteral, "0x39" },
                       { TokenType::Punctuation, ";" },
                       { TokenType::Identifier, "x" },
                       { TokenType::Operator, "=" },
                       { TokenType::IntegerLiteral, "0x39" },
                       { TokenType::Punctuation, ";" },
                       { TokenType::IntegerLiteral, "0x39e" },
                       { TokenType::Punctuation, ";" },
                       { TokenType::Identifier, "x" },
                       { TokenType::Operator, "=" },
                       { TokenType::IntegerLiteral, "0x39e" },
                       { TokenType::Punctuation, ";" },
                       { TokenType::Identifier, "f" },
                       { TokenType::Punctuation, "(" },
                       { TokenType::IntegerLiteral, "39" },
                       { TokenType::Punctuation, ")" },
                       { TokenType::Punctuation, ";" },
                       { TokenType::Identifier, "f" },
                       { TokenType::Punctuation, "(" },
                       { TokenType::FloatingPointLiteral, "39.0" },
                       { TokenType::Punctuation, ")" },
                       { TokenType::Punctuation, ";" },
                       { TokenType::Identifier, "f" },
                       { TokenType::Punctuation, "(" },
                       { TokenType::BooleanLiteral, "true" },
                       { TokenType::Punctuation, ")" },
                       { TokenType::Punctuation, ";" },
                       { TokenType::BooleanLiteral, "false" },
                       { TokenType::Punctuation, ";" },
                       { TokenType::BooleanLiteral, "true" },
                       { TokenType::EndOfFile, "" } };
    test.run();
}

TEST_CASE("Lexer negative", "[lex][issue]") {
    auto issues = test::getLexicalIssues(R"(
123someID
123.23someID
"begin string
and end on next, line"
)");
    issues.findOnLine<InvalidNumericLiteral>(2);
    issues.findOnLine<InvalidNumericLiteral>(3);
    issues.findOnLine<UnterminatedStringLiteral>(4);
}

template <typename T>
static T lexTo(std::string_view text) {
    issue::LexicalIssueHandler iss;
    auto tokens = lex::lex(text, iss);
    assert(iss.empty());
    if constexpr (std::integral<T>) {
        return tokens.front().toInteger();
    }
    else {
        static_assert(std::floating_point<T>);
        return tokens.front().toFloat();
    }
}

#warning Known failure
//TEST_CASE("Lexer float literals", "[lex]") {
//    CHECK(lexTo<double>("1.3") == 1.3);
//    CHECK(lexTo<double>("2.3") == 2.3);
//}
