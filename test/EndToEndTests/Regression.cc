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
