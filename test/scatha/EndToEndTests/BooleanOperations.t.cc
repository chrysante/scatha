#include <catch2/catch_test_macros.hpp>

#include "EndToEndTests/PassTesting.h"

using namespace scatha;

TEST_CASE("Return boolean literal", "[end-to-end]") {
    test::runReturnsTest(1, R"(
fn main() -> bool {
    return true;
})");
}

TEST_CASE("Logical not", "[end-to-end]") {
    test::runReturnsTest(1, R"(
fn main() -> bool {
    let i = 0;
    return !(i == 1);
})");
}

TEST_CASE("Logical and", "[end-to-end]") {
    test::runReturnsTest(0, R"(
fn main() -> bool {
    let a = true;
    let b = false;
    return a && b;
})");
}

TEST_CASE("Logical or", "[end-to-end]") {
    test::runReturnsTest(1, R"(
fn main() -> bool {
    let a = true;
    let b = false;
    return a || b;
})");
}
