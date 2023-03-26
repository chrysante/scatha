#include <Catch/Catch2.hpp>

#include "test/EndToEndTests/BasicCompiler.h"

using namespace scatha;

TEST_CASE("Cast ints and floats", "[end-to-end][casts]") {
    test::checkReturns(10, R"(
fn pow(x: float, n: int) -> float {
    var result = 1.0;
    for i = 0; i < n; ++i {
        result *= x;
    }
    return result;
}
fn main() -> int {
    let x = pow(1.61, int(5.5));
    return int(x);
})");
}
