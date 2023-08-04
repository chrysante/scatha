#include <Catch/Catch2.hpp>

#include <iostream>

#include "Common/APFloat.h"
#include "Parser/Lexer.h"
#include "Parser/LexicalIssue.h"
#include "test/IssueHelper.h"

using namespace scatha;
using namespace parse;

namespace {
struct ReferenceToken {
    TokenKind kind;
    std::string id;
};

std::ostream& operator<<(std::ostream& str, ReferenceToken const& t) {
    return str << "{ .type = " << /*t.type << */ ", .id = " << t.id << " }";
}

struct TestCase {
    std::string text;
    std::vector<ReferenceToken> reference;
    void run() const {
        IssueHandler iss;
        auto const tokens = parse::lex(text, iss);
        REQUIRE(iss.empty());
        REQUIRE(tokens.size() == reference.size());
        for (std::size_t i = 0; i < std::min(tokens.size(), reference.size());
             ++i)
        {
            INFO("LHS: " << reference[i] << "\nRHS: " << tokens[i].id());
            CHECK(reference[i].kind == tokens[i].kind());
            CHECK(reference[i].id == tokens[i].id());
        }
    }
};

} // namespace

TEST_CASE("Lexer positive 1", "[lex]") {
    TestCase test;
    test.text = R"(
fn mul(a: int, b: int) -> int {
	var result: int = a;
	result *= b; return result;
}
)";
    test.reference = { { TokenKind::Function, "fn" },
                       { TokenKind::Identifier, "mul" },
                       { TokenKind::OpenParan, "(" },
                       { TokenKind::Identifier, "a" },
                       { TokenKind::Colon, ":" },
                       { TokenKind::Int, "int" },
                       { TokenKind::Comma, "," },
                       { TokenKind::Identifier, "b" },
                       { TokenKind::Colon, ":" },
                       { TokenKind::Int, "int" },
                       { TokenKind::CloseParan, ")" },
                       { TokenKind::Arrow, "->" },
                       { TokenKind::Int, "int" },
                       { TokenKind::OpenBrace, "{" },
                       { TokenKind::Var, "var" },
                       { TokenKind::Identifier, "result" },
                       { TokenKind::Colon, ":" },
                       { TokenKind::Int, "int" },
                       { TokenKind::Assign, "=" },
                       { TokenKind::Identifier, "a" },
                       { TokenKind::Semicolon, ";" },
                       { TokenKind::Identifier, "result" },
                       { TokenKind::MultipliesAssign, "*=" },
                       { TokenKind::Identifier, "b" },
                       { TokenKind::Semicolon, ";" },
                       { TokenKind::Return, "return" },
                       { TokenKind::Identifier, "result" },
                       { TokenKind::Semicolon, ";" },
                       { TokenKind::CloseBrace, "}" },
                       { TokenKind::EndOfFile, "" } };
    test.run();
}

TEST_CASE("Lexer positive 2", "[lex]") {
    TestCase test;
    test.text = R"(
import std;
import myLib;

fn main() {
	var text: string = f();
	std.print(text);
}
)";
    test.reference = {
        { TokenKind::Import, "import" },     { TokenKind::Identifier, "std" },
        { TokenKind::Semicolon, ";" },       { TokenKind::Import, "import" },
        { TokenKind::Identifier, "myLib" },  { TokenKind::Semicolon, ";" },
        { TokenKind::Function, "fn" },       { TokenKind::Identifier, "main" },
        { TokenKind::OpenParan, "(" },       { TokenKind::CloseParan, ")" },
        { TokenKind::OpenBrace, "{" },       { TokenKind::Var, "var" },
        { TokenKind::Identifier, "text" },   { TokenKind::Colon, ":" },
        { TokenKind::Identifier, "string" }, { TokenKind::Assign, "=" },
        { TokenKind::Identifier, "f" },      { TokenKind::OpenParan, "(" },
        { TokenKind::CloseParan, ")" },      { TokenKind::Semicolon, ";" },
        { TokenKind::Identifier, "std" },    { TokenKind::Dot, "." },
        { TokenKind::Identifier, "print" },  { TokenKind::OpenParan, "(" },
        { TokenKind::Identifier, "text" },   { TokenKind::CloseParan, ")" },
        { TokenKind::Semicolon, ";" },       { TokenKind::CloseBrace, "}" },
        { TokenKind::EndOfFile, "" }
    };
    test.run();
}

TEST_CASE("Lexer positive 3", "[lex]") {
    TestCase test;
    test.text = R"(
a*=b;x+=1;fn(true&&false)+=NULL;
while (x >= 0) {
	x -= x % 3  ? 1 : 2;
}
)";
    test.reference = { { TokenKind::Identifier, "a" },
                       { TokenKind::MultipliesAssign, "*=" },
                       { TokenKind::Identifier, "b" },
                       { TokenKind::Semicolon, ";" },
                       { TokenKind::Identifier, "x" },
                       { TokenKind::PlusAssign, "+=" },
                       { TokenKind::IntegerLiteral, "1" },
                       { TokenKind::Semicolon, ";" },
                       { TokenKind::Function, "fn" },
                       { TokenKind::OpenParan, "(" },
                       { TokenKind::True, "true" },
                       { TokenKind::LogicalAnd, "&&" },
                       { TokenKind::False, "false" },
                       { TokenKind::CloseParan, ")" },
                       { TokenKind::PlusAssign, "+=" },
                       { TokenKind::Identifier, "NULL" },
                       { TokenKind::Semicolon, ";" },
                       { TokenKind::While, "while" },
                       { TokenKind::OpenParan, "(" },
                       { TokenKind::Identifier, "x" },
                       { TokenKind::GreaterEqual, ">=" },
                       { TokenKind::IntegerLiteral, "0" },
                       { TokenKind::CloseParan, ")" },
                       { TokenKind::OpenBrace, "{" },
                       { TokenKind::Identifier, "x" },
                       { TokenKind::MinusAssign, "-=" },
                       { TokenKind::Identifier, "x" },
                       { TokenKind::Remainder, "%" },
                       { TokenKind::IntegerLiteral, "3" },
                       { TokenKind::Question, "?" },
                       { TokenKind::IntegerLiteral, "1" },
                       { TokenKind::Colon, ":" },
                       { TokenKind::IntegerLiteral, "2" },
                       { TokenKind::Semicolon, ";" },
                       { TokenKind::CloseBrace, "}" },
                       { TokenKind::EndOfFile, "" } };
    test.run();
}

TEST_CASE("Lexer positive 4", "[lex]") {
    TestCase test;
    test.text = R"(
import std;
import myLib;

fn main() -> void {
/*
an ignored multi line comment
*/
	var text_ = "Hello World!";
	""
	std.print(--_text);
	++1.0;
}
)";
    test.reference = { { TokenKind::Import, "import" },
                       { TokenKind::Identifier, "std" },
                       { TokenKind::Semicolon, ";" },
                       { TokenKind::Import, "import" },
                       { TokenKind::Identifier, "myLib" },
                       { TokenKind::Semicolon, ";" },
                       { TokenKind::Function, "fn" },
                       { TokenKind::Identifier, "main" },
                       { TokenKind::OpenParan, "(" },
                       { TokenKind::CloseParan, ")" },
                       { TokenKind::Arrow, "->" },
                       { TokenKind::Void, "void" },
                       { TokenKind::OpenBrace, "{" },
                       { TokenKind::Var, "var" },
                       { TokenKind::Identifier, "text_" },
                       { TokenKind::Assign, "=" },
                       { TokenKind::StringLiteral, "Hello World!" },
                       { TokenKind::Semicolon, ";" },
                       { TokenKind::StringLiteral, "" },
                       { TokenKind::Identifier, "std" },
                       { TokenKind::Dot, "." },
                       { TokenKind::Identifier, "print" },
                       { TokenKind::OpenParan, "(" },
                       { TokenKind::Decrement, "--" },
                       { TokenKind::Identifier, "_text" },
                       { TokenKind::CloseParan, ")" },
                       { TokenKind::Semicolon, ";" },
                       { TokenKind::Increment, "++" },
                       { TokenKind::FloatLiteral, "1.0" },
                       { TokenKind::Semicolon, ";" },
                       { TokenKind::CloseBrace, "}" },
                       { TokenKind::EndOfFile, "" } };
    test.run();
}

TEST_CASE("Lexer literals", "[lex]") {
    return;
    TestCase test;
    test.text = R"(
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
    test.reference = { { TokenKind::FloatLiteral, "0.0" },
                       { TokenKind::IntegerLiteral, "39" },
                       { TokenKind::Semicolon, ";" },
                       { TokenKind::Identifier, "x" },
                       { TokenKind::Assign, "=" },
                       { TokenKind::IntegerLiteral, "39" },
                       { TokenKind::Semicolon, ";" },
                       { TokenKind::IntegerLiteral, "0x39" },
                       { TokenKind::Semicolon, ";" },
                       { TokenKind::Identifier, "x" },
                       { TokenKind::Assign, "=" },
                       { TokenKind::IntegerLiteral, "0x39" },
                       { TokenKind::Semicolon, ";" },
                       { TokenKind::IntegerLiteral, "0x39e" },
                       { TokenKind::Semicolon, ";" },
                       { TokenKind::Identifier, "x" },
                       { TokenKind::Assign, "=" },
                       { TokenKind::IntegerLiteral, "0x39e" },
                       { TokenKind::Semicolon, ";" },
                       { TokenKind::Identifier, "f" },
                       { TokenKind::OpenParan, "(" },
                       { TokenKind::IntegerLiteral, "39" },
                       { TokenKind::CloseParan, ")" },
                       { TokenKind::Semicolon, ";" },
                       { TokenKind::Identifier, "f" },
                       { TokenKind::OpenParan, "(" },
                       { TokenKind::FloatLiteral, "39.0" },
                       { TokenKind::CloseParan, ")" },
                       { TokenKind::Semicolon, ";" },
                       { TokenKind::Identifier, "f" },
                       { TokenKind::OpenParan, "(" },
                       { TokenKind::True, "true" },
                       { TokenKind::CloseParan, ")" },
                       { TokenKind::Semicolon, ";" },
                       { TokenKind::False, "false" },
                       { TokenKind::Semicolon, ";" },
                       { TokenKind::True, "true" },
                       { TokenKind::EndOfFile, "" } };
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
    IssueHandler iss;
    auto tokens = parse::lex(text, iss);
    assert(iss.empty());
    if constexpr (std::integral<T>) {
        return tokens.front().toInteger(64).to<u64>();
    }
    else {
        static_assert(std::floating_point<T>);
        return tokens.front().toFloat().to<f64>();
    }
}

TEST_CASE("Lexer float literals", "[lex]") {
    CHECK(lexTo<double>("1.3") == 1.3);
    CHECK(lexTo<double>("2.3") == 2.3);
}

TEST_CASE("String literals", "[lex]") {
    IssueHandler iss;
    SECTION("Simple unterminated") {
        auto const text = R"(")";
        auto tokens = parse::lex(text, iss);
        REQUIRE(!iss.empty());
        auto* issue =
            dynamic_cast<UnterminatedStringLiteral const*>(&iss.front());
        CHECK(issue);
    }
}

TEST_CASE("Escape sequences", "[lex]") {
    IssueHandler iss;
    SECTION("Simple hello world") {
        auto const text = R"("Hello world!\n")";
        auto tokens = parse::lex(text, iss);
        REQUIRE(tokens.size() == 2);
        auto str = tokens.front();
        CHECK(str.id() == "Hello world!\n");
        CHECK(iss.empty());
    }
    SECTION("Simple hello world 2") {
        auto const text = R"("Hello\tworld!")";
        auto tokens = parse::lex(text, iss);
        REQUIRE(tokens.size() == 2);
        auto str = tokens.front();
        CHECK(str.id() == "Hello\tworld!");
        CHECK(iss.empty());
    }
    SECTION("Invalid sequence") {
        auto const text = R"("Hello,\m world!")";
        auto tokens = parse::lex(text, iss);
        REQUIRE(tokens.size() == 2);
        CHECK(!iss.empty());
        CHECK(dynamic_cast<InvalidEscapeSequence const*>(&iss.front()));
        auto str = tokens.front();
        CHECK(str.id() == "Hello,m world!");
    }
    SECTION("Invalid sequence at begin") {
        auto const text = R"("\zHello world!")";
        auto tokens = parse::lex(text, iss);
        REQUIRE(tokens.size() == 2);
        CHECK(!iss.empty());
        CHECK(dynamic_cast<InvalidEscapeSequence const*>(&iss.front()));
        auto str = tokens.front();
        CHECK(str.id() == "zHello world!");
    }
    SECTION("Invalid sequence at end") {
        auto const text = R"("Hello world!\m")";
        auto tokens = parse::lex(text, iss);
        REQUIRE(tokens.size() == 2);
        CHECK(!iss.empty());
        CHECK(dynamic_cast<InvalidEscapeSequence const*>(&iss.front()));
        auto str = tokens.front();
        CHECK(str.id() == "Hello world!m");
    }
}

TEST_CASE("Char literals", "[lex]") {
    IssueHandler iss;
    SECTION("Simple char literal") {
        auto const text = R"('L')";
        auto tokens = parse::lex(text, iss);
        REQUIRE(tokens.size() == 2);
        auto lit = tokens.front();
        CHECK(lit.kind() == TokenKind::CharLiteral);
        CHECK(lit.id() == "L");
        CHECK(iss.empty());
    }
    SECTION("Unterminated") {
        auto const text = R"('L)";
        auto tokens = parse::lex(text, iss);
        REQUIRE(!iss.empty());
        auto* issue =
            dynamic_cast<UnterminatedCharLiteral const*>(&iss.front());
        CHECK(issue);
    }
    SECTION("Unterminated - 2") {
        auto const text = "\'x\n\'";
        auto tokens = parse::lex(text, iss);
        REQUIRE(!iss.empty());
        auto* issue =
            dynamic_cast<UnterminatedCharLiteral const*>(&iss.front());
        CHECK(issue);
    }
    SECTION("Invalid") {
        auto const text = R"('hello world')";
        auto tokens = parse::lex(text, iss);
        REQUIRE(!iss.empty());
        auto* issue = dynamic_cast<InvalidCharLiteral const*>(&iss.front());
        CHECK(issue);
    }
    SECTION("Escape sequence") {
        auto const text = R"('\n')";
        auto tokens = parse::lex(text, iss);
        REQUIRE(tokens.size() == 2);
        auto lit = tokens.front();
        CHECK(lit.kind() == TokenKind::CharLiteral);
        CHECK(lit.id() == "\n");
        CHECK(iss.empty());
    }
    SECTION("Invalid escape sequence") {
        auto const text = R"('\M')";
        auto tokens = parse::lex(text, iss);
        REQUIRE(!iss.empty());
        auto* issue = dynamic_cast<InvalidEscapeSequence const*>(&iss.front());
        CHECK(issue);
    }
}
