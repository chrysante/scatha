#include <Catch/Catch2.hpp>

#include "test/EndToEndTests/BasicCompiler.h"

using namespace scatha;

TEST_CASE("Return boolean literal", "[codegen]") {
    test::checkReturns(1, R"(
fn main() -> bool {
    return true;
})");
}

TEST_CASE("Logical not", "[codegen]") {
    test::checkReturns(1, R"(
fn main() -> bool {
    let i = 0;
    return !(i == 1);
})");
}

TEST_CASE("Logical and", "[codegen]") {
    test::checkReturns(0, R"(
fn main() -> bool {
    let a = true;
    let b = false;
    return a && b;
})");
}

TEST_CASE("Logical or", "[codegen]") {
    test::checkReturns(1, R"(
fn main() -> bool {
    let a = true;
    let b = false;
    return a || b;
})");
}
