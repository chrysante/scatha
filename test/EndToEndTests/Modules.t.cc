#include <catch2/catch_test_macros.hpp>

#include "test/EndToEndTests/PassTesting.h"

using namespace scatha;

TEST_CASE("First multi file program", "[end-to-end][modules]") {
    test::checkReturns(1,
                       R"(
/// File 1
fn main() { return f(); }
)",
                       R"(
/// File 2
fn f() { return 1; }
)");
}

TEST_CASE("Reusing private names", "[end-to-end][modules]") {
    test::checkReturns(6,
                       R"(
/// File 1
private fn impl() { return 2; }
fn main() { return impl() * f(); }
)",
                       R"(
/// File 2
private fn impl() { return 3; }
fn f() { return impl(); }
)");
}
