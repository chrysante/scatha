#include <catch2/catch_test_macros.hpp>

#include "test/EndToEndTests/PassTesting.h"

using namespace scatha;

TEST_CASE("First member function", "[end-to-end][member-functions]") {
    test::checkReturns(1, R"(
struct X {
    fn setValue(&mut this, value: int) {
        this.value = value;
    }
    fn getValue(&this) -> int {
        return this.value;
    }
    var value: int;
}
fn main() -> int {
    var x: X;
    x.value = 0;
    x.setValue(1);
    return x.getValue();
})");
}

TEST_CASE("Uniform call syntax and property calls",
          "[end-to-end][member-functions]") {
    test::checkReturns(42, R"(
struct X {
    fn getValue(&this) -> int {
         return this.value;
    }
    var value: int;
}
fn main() -> int {
    var x: X;
    x.value = 42;
    let result = x.getValue();
    return result;
})");
}
