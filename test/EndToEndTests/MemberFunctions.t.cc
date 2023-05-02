#include <Catch/Catch2.hpp>

#include "test/EndToEndTests/BasicCompiler.h"

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
public fn main() -> int {
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
    var value: int;
}
fn getValue(x: &X) -> int {
     return x.value;
}
public fn main() -> int {
    var x: X;
    x.value = 42;
    let result = x.getValue;
    return result;
})");
}
