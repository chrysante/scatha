#include <catch2/catch_test_macros.hpp>

#include "EndToEndTests/PassTesting.h"
#include "Util/LibUtil.h"

using namespace scatha;
using namespace test;

TEST_CASE("Static library compile and import", "[end-to-end][nativelib]") {
    compileLibrary("libs/testlib1",
                   "libs",
                   R"(
export fn inc(n: &mut int) {
    n += int(__builtin_sqrt_f64(1.0));
})");

    compileLibrary("libs/testlib2",
                   "libs",
                   R"(
import testlib1;
export fn incTwice(n: &mut int) {
    testlib1.inc(n);
    testlib1.inc(n);
})");

    uint64_t ret = compileAndRunDependentProgram("libs",
                                                 R"(
import testlib2;
fn main() -> int {
    var n = 0;
    testlib2.incTwice(n);
    return n;
})");
    CHECK(ret == 2);
}

TEST_CASE("Import native lib in local scope", "[end-to-end][nativelib]") {
    compileLibrary("libs/testlib", "libs", "export fn foo() { return 42; }");
    uint64_t ret = compileAndRunDependentProgram("libs",
                                                 R"(
fn main() -> int {
    import testlib;
    return testlib.foo();
})");
    CHECK(ret == 42);
}

TEST_CASE("Use native lib in local scope", "[end-to-end][nativelib]") {
    compileLibrary("libs/testlib", "libs", "export fn foo() { return 42; }");
    uint64_t ret = compileAndRunDependentProgram("libs",
                                                 R"(
fn main() -> int {
    use testlib;
    return foo();
})");
    CHECK(ret == 42);
}

TEST_CASE("Import native lib twice", "[end-to-end][nativelib]") {
    compileLibrary("libs/testlib", "libs", "export fn foo() { return 42; }");
    uint64_t ret = compileAndRunDependentProgram("libs",
                                                 R"(
fn main() -> int {
    import testlib;
    import testlib;
    return testlib.foo();
})");
    CHECK(ret == 42);
}

TEST_CASE("Import and use native lib twice", "[end-to-end][nativelib]") {
    compileLibrary("libs/testlib", "libs", "export fn foo() { return 42; }");
    uint64_t ret = compileAndRunDependentProgram("libs",
                                                 R"(
fn main() -> int {
    import testlib;
    use testlib;
    return foo();
})");
    CHECK(ret == 42);
}

TEST_CASE("Use native lib twice", "[end-to-end][nativelib]") {
    compileLibrary("libs/testlib", "libs", "export fn foo() { return 42; }");
    uint64_t ret = compileAndRunDependentProgram("libs",
                                                 R"(
fn main() -> int {
    use testlib;
    use testlib;
    return foo();
})");
    CHECK(ret == 42);
}

TEST_CASE("FFI library import", "[end-to-end][foreignlib]") {
    SECTION("foo") {
        CHECK(42 == test::compileAndRun(R"(
import "ffi-testlib";
extern "C" fn foo(n: int, m: int) -> int;
fn main() {
    return foo(22, 20);
})"));
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
        CHECK(42 == test::compileAndRun(R"(
import "ffi-testlib";
extern "C" fn baz() -> void;
fn main() {
    return baz();
})"));
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

TEST_CASE("FFI used by static library", "[end-to-end][nativelib][foreignlib]") {
    compileLibrary("libs/testlib",
                   "libs",
                   R"(
import "ffi-testlib";
extern "C" fn foo(n: int, m: int) -> int;
export fn fooWrapper(n: int, m: int) {
     return foo(n, m);
})");
    uint64_t ret = compileAndRunDependentProgram("libs",
                                                 R"(
import testlib;
fn main() -> int {
    return testlib.fooWrapper(20, 22);
})");
    CHECK(ret == 42);
}
