#include <catch2/catch_test_macros.hpp>

#include "EndToEndTests/PassTesting.h"

using namespace scatha;

TEST_CASE("First dynamic call", "[end-to-end]") {
    test::runReturnsTest(42, R"(
protocol P { fn test(&this) -> int; }
struct S: P {
    fn test(&dyn this) -> int { return this.value; }
    var value: int;
}
fn f(p: &dyn mut P) -> int {
    return p.test();
}
fn main() {
    var s = S(42);
    return f(s);
}
)");
}
