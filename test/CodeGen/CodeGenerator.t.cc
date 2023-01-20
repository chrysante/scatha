#include <Catch/Catch2.hpp>

#include <string>

#include <utl/bit.hpp>

#include "test/CodeGen/BasicCompiler.h"

using namespace scatha;

TEST_CASE("First entire compilation and execution", "[codegen]") {
    test::checkReturns(3, R"(
fn main() -> int {
    let a = 1;
    let b = 2;
    return a + b;
})");
}

TEST_CASE("Simplest non-trivial program", "[codegen]") {
    test::checkReturns(1, R"(
fn main() -> int {
    return 1;
})");
}

TEST_CASE("Addition", "[codegen]") {
    test::checkReturns(3, R"(
fn main() -> int {
    let a = 1;
    let b = 2;
    return a + b;
})");
}

TEST_CASE("Subtraction", "[codegen]") {
    test::checkReturns(static_cast<u64>(-1), R"(
fn main() -> int {
    let a = 1;
    let b = 2;
    return a - b;
})");
}

TEST_CASE("Multiplication", "[codegen]") {
    test::checkReturns(static_cast<u64>(-92), R"(
fn main() -> int {
    let a = 4;
    let b = -23;
    return a * b;
})");
}

TEST_CASE("Division", "[codegen]") {
    test::checkReturns(25, R"(
fn main() -> int {
    let a = 100;
    let b = 4;
    return a / b;
})");
}

TEST_CASE("Remainder", "[codegen]") {
    test::checkReturns(15, R"(
fn main() -> int {
    let a = 100;
    let b = 17;
    return a % b;
})");
}

TEST_CASE("Float Addition", "[codegen]") {
    test::checkReturns(utl::bit_cast<u64>(1.3 + 2.3), R"(
fn test() -> float {
    let a = 1.3;
    let b = 2.3;
    return a + b;
})");
}

TEST_CASE("Float Mutliplication", "[codegen]") {
    test::checkReturns(utl::bit_cast<u64>(1.3 * 2.3), R"(
fn test() -> float {
    let a = 1.3;
    let b = 2.3;
    return a * b;
})");
}

TEST_CASE("Float Subtraction", "[codegen]") {
    test::checkReturns(utl::bit_cast<u64>(1.4 - 2.3), R"(
fn test() -> float {
    let a = 1.4;
    let b = 2.3;
    return a - b;
})");
}

TEST_CASE("Float Division", "[codegen]") {
    test::checkReturns(utl::bit_cast<u64>(1.4 / 2.3), R"(
fn test() -> float {
    let a = 1.4;
    let b = 2.3;
    return a / b;
})");
}

TEST_CASE("More complex expressions", "[codegen]") {
    test::checkReturns(10, R"(
fn main() -> int {
    let a = 12;
    let b = 2;
    let c = 4;
    return (a + b * c) / 2;
})");
}

TEST_CASE("Even more complex expressions", "[codegen]") {
    test::checkReturns(10, R"(
fn main() -> int {
    let a = 12;
    var b = 0;
    let c = 4;
    b += 2;
    return 0, (a + b * c) / 2;
})");
}
