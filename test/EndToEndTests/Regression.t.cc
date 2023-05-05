#include <Catch/Catch2.hpp>

#include "test/EndToEndTests/BasicCompiler.h"

using namespace scatha;

TEST_CASE("CodeGen DCE wrongly eliminates function calls with side effects",
          "[end-to-end][regression]") {
    test::checkReturns(10, R"(
public fn main() -> int {
    var i = 0;
    modifyWithIgnoredReturnValue(&mut i);
    return i;
}
fn modifyWithIgnoredReturnValue(n: &int) -> int {
    n = 10;
    return 0;
})");
}

TEST_CASE("Assignment error", "[end-to-end][regression][arrays]") {
    test::checkReturns(1, R"(
fn f(a: &mut [int], b: &[int]) -> void {
    a[0] = b[0];
}
public fn main() -> int {
    var a = [0, 0];
    var b = [1, 2];
    f(&mut a, &b);
    return a[0];
})");
}
