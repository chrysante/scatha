#include <bit>
#include <cmath>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include "EndToEndTests/PassTesting.h"

using namespace scatha;

TEST_CASE("Slighty complex call graph", "[end-to-end][inlining]") {
    test::runReturnsTest(1, R"(
fn main() -> int {
    return f(1);
}
fn f(n: int) -> int {
    return n > zero() ? pass(n) : zero();
}
fn pass(n: int) -> int { return n; }
fn zero() -> int { return 0; }
fn g(n: int) -> int {
    return n > 0 ? n : 0;
})");
}

TEST_CASE("External function call", "[end-to-end][inlining]") {
    test::runReturnsTest(std::bit_cast<uint64_t>(std::sqrt(2.0)), R"(
fn main() -> double {
    return sqrt(2.0);
}
fn sqrt(x: double) -> double {
    return __builtin_sqrt_f64(x);
})");
}
