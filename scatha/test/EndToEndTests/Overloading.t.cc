#include <catch2/catch_test_macros.hpp>

#include "EndToEndTests/PassTesting.h"

using namespace scatha;

TEST_CASE("Overloading", "[end-to-end]") {
    test::runReturnsTest(1 + 1 + 2 + 3 + 4 + 5 + 6, R"(
fn main() -> int {
    return add() + add(1.0) * add(1) + add(2, 3) + add(4, 5, 6);
}
fn add() -> int {
    return 1;
}
fn add(f: double) -> int {
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

TEST_CASE("Overloading 2", "[end-to-end]") {
    test::runReturnsTest(2, R"(
fn f(i: int, b: bool) -> int { return 1; }
fn f(i: double, b: bool) -> int { return 2; }
fn f(i: bool, b: bool) -> int { return 3; }
fn main() -> int {
    return f(0.0, true);
})");
}

TEST_CASE("Overload on mutability", "[end-to-end]") {
    test::runReturnsTest(0b100, R"(
fn f(value: &int) -> int {
    return 0;
}
fn f(value: &mut int) -> int {
    return 1;
}
fn main() -> int {
    var result = 0;
    let i: int;
    var j: int;
    result |= f(0) << 0;
    result |= f(i) << 1;
    result |= f(j) << 2;
    return result;
})");
}
