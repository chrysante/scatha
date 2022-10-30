#include <Catch/Catch2.hpp>

#include "AST/AST.h"

#include "Lexer/Lexer.h"
#include "Parser/TokenStream.h"
#include "test/Parser/SimpleParser.h"

using namespace scatha;
using namespace parse;


TEST_CASE("TokenStream::advanceUntilStable - To ';'", "[parse]") {
    auto const text = R"(
{ i = ,; }
)";
    TokenStream tokens = test::makeTokenStream(text);
    while (tokens.peek().id != ",") { tokens.eat(); }
    tokens.advanceUntilStable();
    CHECK(tokens.peek().isSeparator);
    CHECK(tokens.peek().id == ";");
}

TEST_CASE("TokenStream::advanceUntilStable - To ';' past '{}'", "[parse]") {
    auto const text = R"(
i = ,{{,}};
j = 0;
)";
    TokenStream tokens = test::makeTokenStream(text);
    while (tokens.peek().id != ",") { tokens.eat(); }
    tokens.advanceUntilStable();
    CHECK(tokens.peek().isSeparator);
    CHECK(tokens.peek().id == ";");
    tokens.eat();
    CHECK(tokens.peek().id == "j");
}

TEST_CASE("TokenStream::advanceUntilStable - To ';' past '(){}'", "[parse]") {
    auto const text = R"(
i = ,{(,)};
j = 0;
)";
    TokenStream tokens = test::makeTokenStream(text);
    while (tokens.peek().id != ",") { tokens.eat(); }
    tokens.advanceUntilStable();
    CHECK(tokens.peek().isSeparator);
    CHECK(tokens.peek().id == ";");
    tokens.eat();
    CHECK(tokens.peek().id == "j");
}

TEST_CASE("TokenStream::advanceUntilStable - To '}'", "[parse]") {
    auto const text = R"(
{ i = , }
)";
    TokenStream tokens = test::makeTokenStream(text);
    while (tokens.peek().id != ",") { tokens.eat(); }
    tokens.advanceUntilStable();
    CHECK(tokens.peek().id == "}");
}

TEST_CASE("TokenStream::advanceUntilStable - To '}' past '{}'", "[parse]") {
    auto const text = R"(
{ i = ,{{,}} }
)";
    TokenStream tokens = test::makeTokenStream(text);
    while (tokens.peek().id != ",") { tokens.eat(); }
    tokens.advanceUntilStable();
    CHECK(tokens.peek().id == "}");
    tokens.eat();
    CHECK(tokens.peek().type == TokenType::EndOfFile);
}

TEST_CASE("TokenStream::advanceUntilStable - To declarator", "[parse]") {
    auto const text = R"(
let j = ,
let i = j;
)";
    TokenStream tokens = test::makeTokenStream(text);
    while (tokens.peek().id != ",") { tokens.eat(); }
    tokens.advanceUntilStable();
    CHECK(tokens.peek().isDeclarator);
    CHECK(tokens.peek().id == "let");
}

TEST_CASE("TokenStream::advanceUntilStable - To declarator from declarator", "[parse]") {
    auto const text = R"(
var j = ,
let i = j;
)";
    TokenStream tokens = test::makeTokenStream(text);
    while (tokens.peek().id != "let") { tokens.eat(); }
    tokens.advanceUntilStable();
    CHECK(tokens.peek().isDeclarator);
    CHECK(tokens.peek().id == "let");
}

TEST_CASE("TokenStream::advanceUntilStable - To declarator past '{}'", "[parse]") {
    auto const text = R"(
{
i = ,{{,}}
var i = 0;
}
)";
    TokenStream tokens = test::makeTokenStream(text);
    while (tokens.peek().id != ",") { tokens.eat(); }
    tokens.advanceUntilStable();
    CHECK(tokens.peek().isDeclarator);
    CHECK(tokens.peek().id == "var");
}
