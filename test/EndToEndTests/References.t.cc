#include <Catch/Catch2.hpp>

#include "test/EndToEndTests/BasicCompiler.h"

using namespace scatha;

TEST_CASE("First reference parameter", "[end-to-end][references]") {
    test::checkReturns(4, R"(
public fn main() -> int {
    var i = 3;
    f(&i);
    return i;
}
fn f(x: &mut int)  {
    x += 1;
})");
}

TEST_CASE("Rebind reference", "[end-to-end][references]") {
    test::checkReturns(2, R"(
public fn main() -> int {
    var i = 0;
    var j = 0;
    var r = &i;
    r += 1;
    r = &j;
    r += 1;
    return i + j;
})");
}
