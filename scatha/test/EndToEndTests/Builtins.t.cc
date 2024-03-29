#include <bit>
#include <cmath>

#include <catch2/catch_test_macros.hpp>

#include "EndToEndTests/PassTesting.h"

using namespace scatha;

TEST_CASE("Common math functions", "[end-to-end]") {
    test::runReturnsTest(std::bit_cast<uint64_t>(std::sqrt(5178)), R"(
fn main() -> double {
    let arg = 5178.0;
    return __builtin_sqrt_f64(arg);
})");
    test::runReturnsTest(std::bit_cast<uint64_t>(std::pow(5.2, 3.4)), R"(
fn main() -> double {
    return __builtin_pow_f64(5.2, 3.4);
})");
    test::runReturnsTest(std::bit_cast<uint64_t>(std::pow(4.8, 10)), R"(
fn main() -> double {
    return __builtin_exp10_f64(4.8);
})");
    test::runReturnsTest(std::bit_cast<uint64_t>(std::sin(1234)), R"(
fn main() -> double {
    return __builtin_sin_f64(1234.0);
})");
}

TEST_CASE("Cast ints and doubles", "[end-to-end][casts]") {
    test::runReturnsTest(10, R"(
fn pow(x: double, n: int) -> double {
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
