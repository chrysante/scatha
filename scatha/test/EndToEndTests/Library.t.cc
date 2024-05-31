#include <catch2/catch_test_macros.hpp>

#include "EndToEndTests/PassTesting.h"
#include "Util/LibUtil.h"

using namespace scatha;
using namespace test;

TEST_CASE("Static library compile and import", "[end-to-end][lib][nativelib]") {
    compileLibrary("libs/testlib1", "libs",
                   R"(
public fn inc(n: &mut int) {
    n += int(__builtin_sqrt_f64(1.0));
})");

    compileLibrary("libs/testlib2", "libs",
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

TEST_CASE("Use global variables from library", "[end-to-end][lib][nativelib]") {
    compileLibrary("libs/testlib", "libs", R"(
public let X: int = 7;
public var Y: int = 0;
public struct ComplexType {
    internal fn new(&mut this, value: int) { this.value = value; }
    fn get(&this) -> int { return this.value; }
    private var value: int;
}
fn computeValue() -> ComplexType { return ComplexType(7 * 42); }
public var ComplexValue: ComplexType = computeValue();
)");
    uint64_t ret = compileAndRunDependentProgram("libs", R"(
import testlib;
use testlib.X;
use testlib.ComplexValue;
fn main() -> int {
    testlib.Y += 42;
    return X + testlib.Y + ComplexValue.get();
})");
    CHECK(ret == 7 + 42 + 7 * 42);
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

TEST_CASE("FFI library nested name", "[end-to-end][lib][foreignlib]") {
    CHECK(42 == test::compileAndRun(R"(
import "nested/ffi-testlib";
extern "C" fn foo(n: int, m: int) -> int;
fn main() {
    return foo(22, 20);
})"));
}

TEST_CASE("FFI pass null pointer", "[end-to-end][lib][foreignlib]") {
    CHECK(1 == test::compileAndRun(R"(
import "ffi-testlib";
extern "C" fn isNull(p: *int) -> bool;
fn main() -> int {
    let i: *int;
    return isNull(i) ? 1 : 0;
})"));
}

TEST_CASE("FFI pass nonnull pointer", "[end-to-end][lib][foreignlib]") {
    CHECK(0 == test::compileAndRun(R"(
import "ffi-testlib";
extern "C" fn isNull(p: *int) -> bool;
fn main() -> int {
    let i: int;
    return isNull(&i) ? 1 : 0;
})"));
}

TEST_CASE("FFI used by static library",
          "[end-to-end][lib][nativelib][foreignlib]") {
    compileLibrary("libs/testlib", "libs",
                   R"(
import "ffi-testlib";
public struct MyStruct { var value: s32; }
extern "C" fn MyStruct_passByValue(s: MyStruct) -> MyStruct;
public fn foo(s: &mut MyStruct) {
    s = MyStruct_passByValue(s);
})");
    uint64_t ret = compileAndRunDependentProgram("libs",
                                                 R"(
import testlib;
fn main() -> int {
    var s = testlib.MyStruct();
    testlib.foo(s);
    return s.value;
})");
    CHECK(ret == 1);
}

#if defined(__GNUC__)
#define SC_TEST_EXPORT __attribute__((visibility("default")))
#else
#error
#endif

TEST_CASE("FFI from host", "[end-to-end][lib][foreignlib]") {
    uint64_t ret = compileAndRunDependentProgram("libs",
                                                 R"(
extern "C" fn host_function(n: int) -> int;
fn main() -> int {
    return host_function(21);
})",
                                                 { .searchHost = true });
    CHECK(ret == 42);
}

/// Defines the function used by `"FFI from host"` test case
SC_TEST_EXPORT extern "C" int64_t host_function(int64_t n) { return 2 * n; }

TEST_CASE("FFI struct passing", "[end-to-end][lib][foreignlib]") {
    uint64_t ret = compileAndRunDependentProgram("libs",
                                                 R"(
struct Params {
    var i: int;
}
extern "C" fn host_function_struct(p: Params) -> Params;
fn main() -> int {
    let p = host_function_struct(Params(1));
    return p.i;
})",
                                                 { .searchHost = true });
    CHECK(ret == 2);
}

struct simple_struct {
    int64_t i;
};

SC_TEST_EXPORT extern "C" simple_struct host_function_struct(simple_struct p) {
    p.i *= 2;
    return p;
}

TEST_CASE("FFI big struct argument", "[end-to-end][lib][foreignlib]") {
    uint64_t ret = compileAndRunDependentProgram("libs",
                                                 R"(
struct Params {
    var i: int;
    var j: int;
    var k: int;
}
extern "C" fn host_function_big_struct_arg(p: Params) -> int;
fn main() -> int {
    return host_function_big_struct_arg(Params(1, 2, 3));
})",
                                                 { .searchHost = true });
    CHECK(ret == 6);
}

struct big_struct {
    int64_t i;
    int64_t j;
    int64_t k;
};

SC_TEST_EXPORT extern "C" int64_t host_function_big_struct_arg(big_struct p) {
    return p.i + p.j + p.k;
}

TEST_CASE("FFI big struct return value", "[end-to-end][lib][foreignlib]") {
    uint64_t ret = compileAndRunDependentProgram("libs",
                                                 R"(
struct Retval {
    var i: int;
    var j: int;
    var k: int;
}
extern "C" fn host_function_big_struct_return() -> Retval;
fn main() -> int {
    let p = host_function_big_struct_return();
    return p.i + p.j + p.k;
})",
                                                 { .searchHost = true });
    CHECK(ret == 6);
}

SC_TEST_EXPORT extern "C" big_struct host_function_big_struct_return() {
    return { 1, 2, 3 };
}

TEST_CASE("Return struct defined in static library",
          "[end-to-end][lib][foreignlib]") {
    compileLibrary("libs/testlib", "libs", R"(
public struct Foo { 
    var x: int;
    var y: int;
})");
    uint64_t ret = compileAndRunDependentProgram("libs", R"(
import testlib;
extern "C" fn return_struct_defined_in_library() -> testlib.Foo;
fn main() -> int {
    // This is a regression test. Returning y made the test fail because the
    // variable index was not correctly deserialized from the library.
    return return_struct_defined_in_library().y;
})",
                                                 { .searchHost = true });
    CHECK(ret == 42);
}

namespace {

struct library_defined_struct {
    int64_t x;
    int64_t y;
};

} // namespace

SC_TEST_EXPORT extern "C" library_defined_struct
    return_struct_defined_in_library() {
    return { .x = 7, .y = 42 };
}

TEST_CASE("FFI nested struct passing", "[end-to-end][lib][foreignlib]") {
    uint64_t ret = compileAndRunDependentProgram("libs",
                                                 R"(
struct InnerStruct {
    var s: s16;
    var f: float;
}
struct ComplexStruct {
    var i: int;
    var f: float;
    var c: s8;
    var d: double;
    var in: InnerStruct;
}
extern "C" fn host_function_complex_struct(p: ComplexStruct) -> ComplexStruct;
fn main() -> bool {
    let p = host_function_complex_struct(ComplexStruct(1, 1.5, 2, 2.5,
                                               InnerStruct(3, 3.5)));
    
    return p.i == 2 && p.f == 3.0 && p.c == 4 && p.d == 5.0 && p.in.s == 6 &&
           p.in.f == 7.0;
})",
                                                 { .searchHost = true });
    CHECK(ret == 1);
}

struct inner_struct {
    short s;
    float f;
};

struct complex_struct {
    int64_t i;
    float f;
    char c;
    double d;
    inner_struct in;
};

SC_TEST_EXPORT extern "C" complex_struct host_function_complex_struct(
    complex_struct p) {
    p.i *= 2;
    p.f *= 2;
    p.c *= 2;
    p.d *= 2;
    p.in.s *= 2;
    p.in.f *= 2;
    return p;
}

TEST_CASE("FFI pointer in big struct", "[end-to-end][lib][foreignlib]") {
    uint64_t ret = compileAndRunDependentProgram("libs",
                                                 R"(
struct X {
    var i: int;
    var string: *str;
}
extern "C" fn host_function_pointer_in_struct(p: X) -> bool;
fn main() -> bool {
    return host_function_pointer_in_struct(X(0, &"Hello World"));
})",
                                                 { .searchHost = true });
    CHECK(ret == 1);
}

struct big_struct_with_pointer {
    int64_t i;
    char const* string;
    size_t string_size;
};

SC_TEST_EXPORT extern "C" bool host_function_pointer_in_struct(
    big_struct_with_pointer s) {
    return std::string_view(s.string, s.string_size) == "Hello World";
}

TEST_CASE("Nested big struct", "[end-to-end][lib][foreignlib]") {
    uint64_t ret = compileAndRunDependentProgram("libs",
                                                 R"(
struct BigInner {
    var x: double;
    var y: double;
    var z: double;
}
struct BigOuter {
    var i: BigInner;
}
extern "C" fn host_function_nested_big_struct(o: BigOuter) -> bool;

fn main() -> bool {
    return host_function_nested_big_struct(BigOuter(BigInner(0.0, 1.5, 100.0)));
}
)",
                                                 { .searchHost = true });
    CHECK(ret == 1);
}

namespace {

struct BigInner {
    double x, y, z;
};
struct BigOuter {
    BigInner i;
};

} // namespace

SC_TEST_EXPORT extern "C" bool host_function_nested_big_struct(BigOuter o) {
    return o.i.x == 0.0 && o.i.y == 1.5 && o.i.z == 100.0;
}
