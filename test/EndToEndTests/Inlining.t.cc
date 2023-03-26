#include <Catch/Catch2.hpp>

#include <string>

#include "test/EndToEndTests/BasicCompiler.h"

using namespace scatha;

TEST_CASE("Slighty complex call graph", "[end-to-end][inlining]") {
    test::checkReturns(1, R"(
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
    test::checkReturns(utl::bit_cast<uint64_t>(std::sqrt(2.0)), R"(
fn main() -> float {
    return sqrt(2.0);
}
fn sqrt(x: float) -> float {
    return __builtin_sqrt_f64(x);
})");
}
