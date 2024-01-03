#include <catch2/catch_test_macros.hpp>

#include "EndToEndTests/PassTesting.h"

using namespace scatha;

TEST_CASE("Bitwise left shift", "[end-to-end]") {
    test::runReturnsTest(303432224, R"(
fn main() -> int {
    return 18964514 << 4;
})");
}

TEST_CASE("Bitwise right shift", "[end-to-end]") {
    test::runReturnsTest(296320, R"(
fn main() -> int {
    return 18964514 >> 6;
})");
}

TEST_CASE("Bitwise AND", "[end-to-end]") {
    test::runReturnsTest(21, R"(
fn main() -> int {
    return 29 & 23;
})");
}

TEST_CASE("Bitwise AND 2", "[end-to-end]") {
    test::runReturnsTest(0, R"(
fn main() -> int {
    return (256 -   2) &
           (256 -   3) &
           (256 -   5) &
           (256 -   9) &
           (256 -  17) &
           (256 -  33) &
           (256 -  65) &
           (256 - 129);
})");
}

TEST_CASE("Bitwise OR", "[end-to-end]") {
    test::runReturnsTest(31, R"(
fn main() -> int {
    return 29 | 23;
})");
}

TEST_CASE("Bitwise OR 2", "[end-to-end]") {
    test::runReturnsTest(0xFF, R"(
fn main() -> int {
    return 0x01 |
           0x02 |
           0x04 |
           0x08 |
           0x10 |
           0x20 |
           0x40 |
           0x80;
})");
}

TEST_CASE("Bitwise OR 3", "[end-to-end]") {
    test::runReturnsTest(127, R"(
fn main() -> int {
    return (1 << 0) |
           (1 << 1) |
           (1 << 2) |
           (1 << 3) |
           (1 << 4) |
           (1 << 5) |
           (1 << 6);
})");
}

TEST_CASE("Bitwise XOR", "[end-to-end]") {
    test::runReturnsTest(10, R"(
fn main() -> int {
    return 29 ^ 23;
})");
}

TEST_CASE("Bitwise XOR 2", "[end-to-end]") {
    test::runReturnsTest(0x00FF00FF00FF, R"(
fn main() -> int {
    return 0xFF00FF00FF00 ^ 0xFFffFFffFFff;
})");
}

TEST_CASE("Bitwise NOT", "[end-to-end]") {
    test::runReturnsTest(~u64(23), R"(
fn main() -> int {
    return ~23;
})");
}

TEST_CASE("Bitwise NOT 2", "[end-to-end]") {
    test::runReturnsTest(0xFFffFFff00FF00FF, R"(
fn main() -> int {
    return ~0xFF00FF00;
})");
}
