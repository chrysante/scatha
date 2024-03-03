#include <catch2/catch_test_macros.hpp>

#include "EndToEndTests/PassTesting.h"

using namespace scatha;

TEST_CASE("Simple constant global", "[end-to-end][globals]") {
    test::runReturnsTest(1, R"(
let i: int = 1;
fn main() {
    return i;
})");
}

TEST_CASE("Simple mutable global", "[end-to-end][globals]") {
    test::runReturnsTest(1, R"(
var i: int = 0;
fn main() {
    ++i;
    return i;
})");
}
