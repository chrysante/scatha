#include <Catch/Catch2.hpp>

#include "test/EndToEndTests/BasicCompiler.h"

using namespace scatha;

TEST_CASE("Overloading", "[codegen]") {
    test::checkReturns(1 + 1 + 2 + 3 + 4 + 5 + 6, R"(
fn main() -> int {
    return add() + add(1.0) * add(1) + add(2, 3) + add(4, 5, 6);
}
fn add() -> int {
    return 1;
}
fn add(f: float) -> int {
    return 1;
}
fn add(x: int) -> int {
    return x;
}
fn add(x: int, y: int) -> int {
    return x + y;
}
fn add(x: int, y: int, z: int) -> int {
    return x + y + z;
})");
}

TEST_CASE("Overloading 2", "[codegen]") {
    test::checkReturns(2, R"(
fn f(i: int, b: bool) -> int { return 1; }
fn f(i: float, b: bool) -> int { return 2; }
fn f(i: bool, b: bool) -> int { return 3; }
fn main() -> int {
    return f(0.0, true);
})");
}
