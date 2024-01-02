#include <catch2/catch_test_macros.hpp>

#include <string>

#include <utl/bit.hpp>

#include "test/EndToEndTests/PassTesting.h"

using namespace scatha;

TEST_CASE("First entire compilation and execution", "[end-to-end]") {
    test::runReturnsTest(3, R"(
fn main() -> int {
    let a = 1;
    let b = 2;
    return a + b;
})");
}

TEST_CASE("Simplest non-trivial program", "[end-to-end]") {
    test::runReturnsTest(1, R"(
fn main() -> int {
    return 1;
})");
}

TEST_CASE("Addition", "[end-to-end]") {
    test::runReturnsTest(3, R"(
fn main() -> int {
    let a = 1;
    let b = 2;
    return a + b;
})");
}

TEST_CASE("Subtraction", "[end-to-end]") {
    test::runReturnsTest(static_cast<u64>(-1), R"(
fn main() -> int {
    let a = 1;
    let b = 2;
    return a - b;
})");
}

TEST_CASE("Multiplication", "[end-to-end]") {
    test::runReturnsTest(static_cast<u64>(-92), R"(
fn main() -> int {
    let a = 4;
    let b = -23;
    return a * b;
})");
}

TEST_CASE("Division", "[end-to-end]") {
    test::runReturnsTest(25, R"(
fn main() -> int {
    let a = 100;
    let b = 4;
    return a / b;
})");
}

TEST_CASE("Remainder", "[end-to-end]") {
    test::runReturnsTest(15, R"(
fn main() -> int {
    let a = 100;
    let b = 17;
    return a % b;
})");
}

TEST_CASE("Float Addition", "[end-to-end]") {
    test::runReturnsTest(utl::bit_cast<u64>(1.3 + 2.3), R"(
fn main() -> double {
    let a = 1.3;
    let b = 2.3;
    return a + b;
})");
}

TEST_CASE("Float Mutliplication", "[end-to-end]") {
    test::runReturnsTest(utl::bit_cast<u64>(1.3 * 2.3), R"(
fn main() -> double {
    let a = 1.3;
    let b = 2.3;
    return a * b;
})");
}

TEST_CASE("Float Subtraction", "[end-to-end]") {
    test::runReturnsTest(utl::bit_cast<u64>(1.4 - 2.3), R"(
fn main() -> double {
    let a = 1.4;
    let b = 2.3;
    return a - b;
})");
}

TEST_CASE("Float Division", "[end-to-end]") {
    test::runReturnsTest(utl::bit_cast<u64>(1.4 / 2.3), R"(
fn main() -> double {
    let a = 1.4;
    let b = 2.3;
    return a / b;
})");
}

TEST_CASE("More complex expressions", "[end-to-end]") {
    test::runReturnsTest(10, R"(
fn main() -> int {
    let a = 12;
    let b = 2;
    let c = 4;
    return (a + b * c) / 2;
})");
}

TEST_CASE("Even more complex expressions", "[end-to-end]") {
    test::runReturnsTest(10, R"(
fn main() -> int {
    let a = 12;
    var b = 0;
    let c = 4;
    b += 2;
    return 0, (a + b * c) / 2;
})");
}

TEST_CASE("Pre-increment/decrement", "[end-to-end]") {
    test::runReturnsTest(1, R"(
fn main() -> int {
    var i = 0;
    var k = ++i;
    return k;
})");
    test::runReturnsTest(1, R"(
fn main() -> int {
    var i = 0;
    var k = ++i;
    return i;
})");
}

TEST_CASE("Post-increment/decrement", "[end-to-end]") {
    test::runReturnsTest(0, R"(
fn main() -> int {
    var i = 0;
    var k = i++;
    return k;
})");
    test::runReturnsTest(1, R"(
fn main() -> int {
    var i = 0;
    var k = i++;
    return i;
})");
}

TEST_CASE("Arithmetic LEA matching", "[end-to-end]") {
    SECTION("1") {
        test::runReturnsTest(42, R"(
//    *    Const
//     \   /
// *    Mul
//  \   /
//   Add2  Const
//     \   /
//      Add1
fn test(a: int, b: int) {
    return a + 10 * b + 20;
}
fn main() {
    return test(2, 2);
})");
    }
    SECTION("2") {
        test::runReturnsTest(42, R"(
//    *    Const
//     \   /
//      Mul   Const
//        \   /
//    *    Add2
//     \   /
//      Add1
fn test(a: int, b: int) {
    return a + (10 * b + 20);
}
fn main() {
    return test(2, 2);
})");
    }
    SECTION("3") {
        test::runReturnsTest(42, R"(
//    *    Const
//     \   /
// *    Mul
//  \   /
//   Add
fn test(a: int, b: int) {
    return a + 10 * b;
}
fn main() {
    return test(2, 4);
})");
    }
    SECTION("4") {
        test::runReturnsTest(42, R"(
// *    Const
//  \   /
//   Mul    *
//     \   /
//      Add
fn test(a: int, b: int) {
    return 10 * b + a;
}
fn main() {
    return test(2, 4);
})");
    }
}
