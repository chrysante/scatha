#include <catch2/catch_test_macros.hpp>

#include "test/EndToEndTests/PassTesting.h"

using namespace scatha;

TEST_CASE("FFI library import", "[end-to-end][member-access]") {
    SECTION("foo") {
        test::runReturnsTest(42, R"(
import "ffi-testlib";

extern "C" fn foo(n: int, m: int) -> int;

fn main() {
    return foo(22, 20);
})");
    }
    SECTION("bar") {
        test::runPrintsTest("bar(7, 11)\n", R"(
import "ffi-testlib";

extern "C" fn bar(n: int, m: int) -> void;

fn main() {
    bar(7, 11);
})");
    }
    SECTION("baz") {
        test::runReturnsTest(42, R"(
import "ffi-testlib";

extern "C" fn baz() -> void;

fn main() {
    return baz();
})");
    }
    SECTION("quux") {
        test::runPrintsTest("quux\n", R"(
import "ffi-testlib";

extern "C" fn quux() -> void;

fn main() {
    quux();
})");
    }
}
