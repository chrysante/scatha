#include <catch2/catch_test_macros.hpp>

#include "EndToEndTests/PassTesting.h"

using namespace scatha;

TEST_CASE("Simple constant global", "[end-to-end][globals]") {
    test::runReturnsTest(1, R"(
let i: int = 1;
fn main() -> int { return i; }
)");
}

TEST_CASE("Simple mutable global", "[end-to-end][globals]") {
    test::runReturnsTest(1, R"(
var i: int = 0;
fn main() -> int { ++i; return i; }
)");
}

TEST_CASE("Complex constant global", "[end-to-end][globals]") {
    test::runReturnsTest(1, R"(
var i: int = 0;
let j: int = i + 1;
fn main() { return j; }
)");
}

TEST_CASE("Complex mutable global", "[end-to-end][globals]") {
    test::checkPrints("ABC", R"(
var i: int = hello();
fn hello() -> int {
    __builtin_putstr("B");
    return 0;
}
fn main() {
    __builtin_putstr("A");
    let r = i;
    __builtin_putstr("C");
})");
}

TEST_CASE("Global array and pointer", "[end-to-end][globals]") {
    test::checkReturns(6, R"(
var a: [int, 3];
let x: int = initA();
let p: *[int] = &a;
fn initA() -> int {
    a = [1, 2, 3];
    return 0;
}
fn main() -> int {
    x;
    return p[0] + p[1] + p[2];
})");
}
