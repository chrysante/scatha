#include <Catch/Catch2.hpp>

#include "test/EndToEndTests/BasicCompiler.h"

using namespace scatha;

TEST_CASE("Return boolean literal", "[end-to-end]") {
    test::checkReturns(1, R"(
public fn main() -> bool {
    return true;
})");
}

TEST_CASE("Logical not", "[end-to-end]") {
    test::checkReturns(1, R"(
public fn main() -> bool {
    let i = 0;
    return !(i == 1);
})");
}

TEST_CASE("Logical and", "[end-to-end]") {
    test::checkReturns(0, R"(
public fn main() -> bool {
    let a = true;
    let b = false;
    return a && b;
})");
}

TEST_CASE("Logical or", "[end-to-end]") {
    test::checkReturns(1, R"(
public fn main() -> bool {
    let a = true;
    let b = false;
    return a || b;
})");
}
