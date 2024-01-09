#include <catch2/catch_test_macros.hpp>

#include "EndToEndTests/PassTesting.h"
#include "Util/LibUtil.h"

using namespace scatha;
using namespace test;

TEST_CASE("Static library compile and import", "[end-to-end][lib][nativelib]") {
    compileLibrary("libs/testlib1",
                   "libs",
                   R"(
public fn inc(n: &mut int) {
    n += int(__builtin_sqrt_f64(1.0));
})");

    compileLibrary("libs/testlib2",
                   "libs",
                   R"(
import testlib1;
public fn incTwice(n: &mut int) {
    testlib1.inc(n);
    testlib1.inc(n);
})");

    uint64_t ret = compileAndRunDependentProgram("libs",
                                                 R"(
import testlib1;
import testlib2;
fn main() -> int {
    var n = 0;
    testlib1.inc(n);
    testlib2.incTwice(n);
    return n;
})");
    CHECK(ret == 3);
}

TEST_CASE("Import native lib in local scope", "[end-to-end][lib][nativelib]") {
    compileLibrary("libs/testlib", "libs", "public fn foo() { return 42; }");
    uint64_t ret = compileAndRunDependentProgram("libs",
                                                 R"(
fn main() -> int {
    import testlib;
    return testlib.foo();
})");
    CHECK(ret == 42);
}

TEST_CASE("Use native lib in local scope", "[end-to-end][lib][nativelib]") {
    compileLibrary("libs/testlib", "libs", "public fn foo() { return 42; }");
    uint64_t ret = compileAndRunDependentProgram("libs",
                                                 R"(
fn main() -> int {
    use testlib;
    return foo();
})");
    CHECK(ret == 42);
}

TEST_CASE("Import native lib twice", "[end-to-end][lib][nativelib]") {
    compileLibrary("libs/testlib", "libs", "public fn foo() { return 42; }");
    uint64_t ret = compileAndRunDependentProgram("libs",
                                                 R"(
fn main() -> int {
    import testlib;
    import testlib;
    return testlib.foo();
})");
    CHECK(ret == 42);
}

TEST_CASE("Import and use native lib twice", "[end-to-end][lib][nativelib]") {
    compileLibrary("libs/testlib", "libs", "public fn foo() { return 42; }");
    uint64_t ret = compileAndRunDependentProgram("libs",
                                                 R"(
fn main() -> int {
    import testlib;
    use testlib;
    return foo();
})");
    CHECK(ret == 42);
}

TEST_CASE("Use native lib twice", "[end-to-end][lib][nativelib]") {
    compileLibrary("libs/testlib", "libs", "public fn foo() { return 42; }");
    uint64_t ret = compileAndRunDependentProgram("libs",
                                                 R"(
fn main() -> int {
    use testlib;
    use testlib;
    return foo();
})");
    CHECK(ret == 42);
}

TEST_CASE("Use special member functions from library",
          "[end-to-end][lib][nativelib]") {
    compileLibrary("libs/testlib", "libs", R"(
public struct X {
    fn new(&mut this) { this.value = 42; }
    var value: int;
})");
    uint64_t ret = compileAndRunDependentProgram("libs", R"(
use testlib.X;
fn main() -> int {
    return X().value;
})");
    CHECK(ret == 42);
}

TEST_CASE("Use overload set by name", "[end-to-end][lib][nativelib]") {
    compileLibrary("libs/testlib", "libs", R"(
public fn foo(n: int) { return 1; }
public fn foo(n: double) { return 2; }
)");
    uint64_t ret = compileAndRunDependentProgram("libs", R"(
use testlib.foo;
fn main() -> int {
    return foo(1) + foo(1.0);
})");
    CHECK(ret == 3);
}

TEST_CASE("Use overload set by name and overload further",
          "[end-to-end][lib][nativelib]") {
    compileLibrary("libs/testlib", "libs", R"(
public fn foo(n: int) { return 1; }
public fn foo(n: double) { return 2; }
)");
    uint64_t ret = compileAndRunDependentProgram("libs", R"(
use testlib.foo;
fn foo(text: &str) { return 3; }
fn main() -> int {
    return foo(1) + foo(1.0) + foo("");
})");
    CHECK(ret == 6);
}

TEST_CASE("Transitive library use", "[end-to-end][lib][nativelib]") {
    compileLibrary("libs/testlib1", "libs", R"(
public struct X {
    var i: int;
})");
    compileLibrary("libs/testlib2", "libs", R"(
use testlib1;
public struct Y {
    fn new(&mut this) { this.x = X(42); }
    var x: X;
})");
    uint64_t ret = compileAndRunDependentProgram("libs", R"(
use testlib2;
fn main() -> int {
    var y = Y();
    return y.x.i;
})");
    CHECK(ret == 42);
}

TEST_CASE("FFI library import", "[end-to-end][lib][foreignlib]") {
    SECTION("foo") {
        CHECK(42 == test::compileAndRun(R"(
import "ffi-testlib";
extern "C" fn foo(n: int, m: int) -> int;
fn main() {
    return foo(22, 20);
})"));
    }
    SECTION("bar") {
        test::checkPrints("bar(7, 11)\n", R"(
import "ffi-testlib";
extern "C" fn bar(n: int, m: int) -> void;
fn main() {
    bar(7, 11);
})");
    }
    SECTION("baz") {
        test::checkReturns(42, R"(
import "ffi-testlib";
extern "C" fn baz() -> int;
fn main() {
    return baz();
})");
    }
    SECTION("quux") {
        test::checkPrints("quux\n", R"(
import "ffi-testlib";
extern "C" fn quux() -> void;
fn main() {
    quux();
})");
    }
    SECTION("Foreign pointers") {
        test::checkReturns(11, R"(
import "ffi-testlib";
extern "C" fn MyStruct_make(value: s32) -> int;
extern "C" fn MyStruct_free(ptr: int) -> void;
extern "C" fn MyStruct_value(ptr: int) -> s32;
fn main() {
    let ptr = MyStruct_make(11);
    let value = MyStruct_value(ptr);
    MyStruct_free(ptr);
    return value;
})");
    }
    SECTION("Pass native pointers to FFI") {
        test::checkPrints("Hello World : Size = 11", R"(
import "ffi-testlib";
extern "C" fn printString(text: *str) -> void;
fn main() {
    printString(&"Hello World");
})");
    }
}

TEST_CASE("FFI used by static library",
          "[end-to-end][lib][nativelib][foreignlib]") {
    compileLibrary("libs/testlib",
                   "libs",
                   R"(
import "ffi-testlib";
extern "C" fn foo(n: int, m: int) -> int;
public fn fooWrapper(n: int, m: int) {
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
