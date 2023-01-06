#include <Catch/Catch2.hpp>

#include "Lexer/ExtractLines.h"

TEST_CASE("ExtractLines", "[lex]") {
    auto const text  = R"(line 0
line 1
line 2
line 3

line 5)";
    auto const lines = scatha::lex::extractLines(text);
    CHECK(lines[0] == "line 0");
    CHECK(lines[1] == "line 1");
    CHECK(lines[2] == "line 2");
    CHECK(lines[3] == "line 3");
    CHECK(lines[4] == "");
    CHECK(lines[5] == "line 5");
}
