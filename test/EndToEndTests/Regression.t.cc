#include <chrono>

#include <catch2/catch_test_macros.hpp>

#include "EndToEndTests/PassTesting.h"

using namespace scatha;

TEST_CASE("CodeGen DCE wrongly eliminates function calls with side effects",
          "[end-to-end][regression]") {
    test::runReturnsTest(10, R"(
fn main() -> int {
    var i = 0;
    modifyWithIgnoredReturnValue(i);
    return i;
}
fn modifyWithIgnoredReturnValue(n: &mut int) -> int {
    n = 10;
    return 0;
})");
}

TEST_CASE("Assignment error", "[end-to-end][regression][arrays]") {
    test::runReturnsTest(1, R"(
fn f(a: &mut [int], b: &[int]) -> void {
    a[0] = b[0];
}
fn main() -> int {
    var a = [0, 0];
    var b = [1, 2];
    f(a, b);
    return a[0];
})");
}

TEST_CASE("Weird bug in simplifyCFG", "[end-to-end][regression]") {
    /// `simplifyCFG()`would crash because an erased basic block was still in
    /// the worklist, so the algorithm would read deallocated memory down the
    /// road. Unfortunately I was not able to simplify the code sample any
    /// further while still reproducing the issue.
    CHECK(test::compiles(R"(
fn main() {
    for i = 1; i < 13; ++i {
        if i != 1 {
            print(", ");
        }
        print(fib(i));
    }
    print("");
}
fn fib(n: int) -> int {
    if (n < 3) {
        return 1;
    }
    return fib(n - 1) + fib(n - 2);
}
fn print(n: int) {
    __builtin_puti64(n);
}
fn print(msg: &str) {
    __builtin_putstr(msg);
})"));
}

TEST_CASE("Bug in LoopRotate - 1", "[end-to-end][regression]") {
    test::runReturnsTest(3, R"(
fn main() -> int {
    var n = 0;
    for i = 0; i < 10; ++i {
        n += 2;
        if n > 10 {
            return n / 4;
        }
    }
    return 0;
})");
}

TEST_CASE("Bug in LoopRotate - 2", "[end-to-end][regression]") {
    test::runReturnsTest(4, R"(
fn main() -> int {
    var sum = 0;
    for i = 0; i < 2; ++i {
        for j = 0; j < 2; ++j {
            sum += i + j;
        }
    }
    return sum;
})");
}

TEST_CASE("Bug in simplifycfg", "[end-to-end][regression]") {
    test::runReturnsTest(10, R"(
fn main() -> int {
    var n = undefInt();
    var cond = undefBool();
    if cond {}
    else {}
    n ^= n;
    n += 10;
    return n;
}
fn undefInt() -> int {}
fn undefBool() -> bool {}
)");
}

TEST_CASE("Size of array data member", "[end-to-end][regression]") {
    test::runReturnsTest(5, R"(
struct X {
    var data: [int, 5];
}
fn main() -> int {
    var x: X;
    return x.data.count;
})");
}

TEST_CASE("Pass large array by value", "[end-to-end][regression]") {
    test::runReturnsTest(1, R"(
fn first(data: [int, 10]) -> int {
    return data[0];
}
fn main() -> int {
    let data = [1, 2, 3, 4, 5, 6, 7, 8, 9, 0];
    return first(data);
})");
}

TEST_CASE("MemToReg bug") {
    test::runIRReturnsTest(42, R"(
func i64 @main() {
  %entry:
    %0 = insert_value [i64, 2] undef, i64 42, 0
    %res = call i64 @f, [i64, 2] %0
    return i64 %res
}
func i64 @f([i64, 2] %0) {
  %entry:
    %data = alloca [i64, 2], i32 1
    store ptr %data, [i64, 2] %0
    %result = load i64, ptr %data
    return i64 %result
})");
}

TEST_CASE("Bug in SSA destruction / register allocation") {
    /// When the arguments to both calls are computed before the first call
    /// instruction, SSA destruction would not realize that the argument
    /// register to the first call must be preserved and would override it with
    /// the computation of the argument to the second call.
    /// This results in wrongfully returning 7 instead of 6, because both calls
    /// to `f` end up with arguments 0 and 3
    test::runIRReturnsTest(6, R"(
func i64 @main-s64(i64 %0) {
  %entry:
    %call.result = call i64 @g-s64, i64 1
    return i64 %call.result
}
func i64 @g-s64(i64 %0) {
  %entry:
    %expr = mul i64 %0, i64 2
    %expr.0 = mul i64 %0, i64 3
    %call.result = call i64 @f-s64-s64, i64 0, i64 %expr
    %call.result.0 = call i64 @f-s64-s64, i64 0, i64 %expr.0
    %expr.1 = add i64 %call.result, i64 1
    %expr.2 = add i64 %expr.1, i64 %call.result.0
    return i64 %expr.2
}
func i64 @f-s64-s64(i64 %0, i64 %1) {
  %entry:
    return i64 %1
})");
}

TEST_CASE("SROA being too aggressive with phi'd pointers speculatively "
          "exectuting stores") {
    test::runReturnsTest(3, R"(
fn main() -> int {
    var cond = true;
    var a = 0;
    var b = 1;
    var c: &mut int = cond ? a : b;
    if !cond {
        c = 2;
    }
    else {
        c = 3;
    }
    return c;
})");
}

TEST_CASE("Invalid code generation for early declared compare operations") {
    test::runIRReturnsTest(3, R"(
func i64 @main() {
%entry:
    %0 = scmp eq i32 0, i32 1
    %1 = scmp eq i32 1, i32 2
    
    %s = select i1 %0, i64 1, i64 2
    %r = select i1 %1, i64 %s, i64 3
    return i64 %r
})");
}

TEST_CASE("Struct member of array type") {
    test::runReturnsTest(4, R"(
struct X {
    var data: [s32, 3];
}
fn main() -> int {
    var x: X;
    x.data[0] = 1;
    return x.data[0] + x.data.count;
})");
}

TEST_CASE("Invalid array size calculation when reinterpreting array pointers "
          "and references") {
    test::runReturnsTest(12, R"(
fn main() -> int {
    let data = [s32(1), s32(2), s32(3)];
    return reinterpret<&[byte]>(data).count;
})");
}

TEST_CASE("Return non-trivial type by reference") {
    test::runReturnsTest(1, R"(
struct X {
    fn new(&mut this, n: int) { this.value = n; }
    fn new(&mut this, rhs: &X) {}
    fn delete(&mut this) {}
    var value: int;
}
fn pass(value: &X) -> &X { return value; }
fn main() {
    return pass(X(1)).value;
})");
}

TEST_CASE("Codegen bug with chained consversions of constants") {
    test::runIRReturnsTest(2, R"(
func i64 @main() {
    %entry:
    %trunc = trunc i64 1 to i8
    %zext = zext i8 %trunc to i64
    %sum = add i64 1, i64 %zext
    return i64 %sum
})");
}

TEST_CASE("Codegen bug with extract_value from undef") {
    CHECK(test::IRCompiles(R"(
func i32 @main() {
  %entry:
    %res = extract_value { i32, i64 } undef, 0
    return i32 %res
})"));
}

TEST_CASE("Codegen bug with gep from undef") {
    CHECK(test::compiles(R"(
struct X { var value: int; }
fn getRef() -> &mut X {}
fn main() {
    return getRef().value;
})"));
}

TEST_CASE("Bug in InstCombine", "[regression]") {
    test::runIRReturnsTest(0, R"(
func i64 @main() {
  %entry:
    %res = srem i64 10, i64 10
    return i64 %res
})");
}

/// SROA used to crash on this program because slice points where computed
/// incorrectly
TEST_CASE("Bug in SROA", "[regression]") {
    test::runReturnsTest(1, R"(
struct Y {
    var a: int;
    var b: int;
}
struct X {
    var a: int;
    var b: int;
    var y: Y;
}
fn main() {
    let x = X(1, 1, Y(1, 1));
    return x.y.b;
})");
}

TEST_CASE("Fat pointer in construct expr", "[regression]") {
    test::runReturnsTest(5, R"(
    struct S {
        var text: *str;
    }
    fn main() {
        let s = S(&"12345");
        return s.text.count;
    })");
}

namespace {

struct Timer {
    using Clock = std::chrono::high_resolution_clock;

    Timer() { reset(); }

    Clock::duration elapsed() const { return Clock::now() - start; }

    void reset() { start = Clock::now(); }

    Clock::time_point start;
};

} // namespace

/// This test case guards against a performance bug in `SelectionDag::Build()`
/// where we compute the dependency sets of each node. Prior to the bugfix the
/// given function would take several seconds to compute the dependency sets,
/// after the fix it should happen almost instantly
TEST_CASE("Performance bug in SelectionDag::Build", "[regression]") {
    Timer t;
    test::compile(R"(
fn foo(data: &[int]) { return true; }

fn test() {
    let data = [1, 2, 3, 4];
    var result = true;
    result &= foo(data);
    result &= foo(data);
    result &= foo(data);
    result &= foo(data);
    result &= foo(data);
    result &= foo(data);
    result &= foo(data);
    result &= foo(data);
    result &= foo(data);
})");
    auto elapsed = t.elapsed();
    using namespace std::chrono_literals;
    CHECK(elapsed < 100ms);
}

TEST_CASE("Unique to raw ptr array size", "[regression]") {
    test::checkReturns(3, R"(
fn main() {
    let p = unique [int](3);
    let q: *[int] = p;
    return q.count;
})");
}

TEST_CASE("Loop increment only used by phi instruction", "[regression]") {
    test::checkIRReturns(10, R"(
func i64 @main() {
  %entry:
    goto label %body

  %body: // preds: entry, body
    %counter = phi i64 [label %entry : 0], [label %body : %ind]
    %ind = add i64 %counter, i64 1
    %cond = scmp neq i64 %counter, i64 10
    branch i1 %cond, label %body, label %end

  %end: // preds: body
    return i64 %counter
})");
}

TEST_CASE("Unique pointer deallocation size", "[end-to-end][regression]") {
    /// This used to crash because all sizes passed to `__builtin_dealloc` were
    /// 8 or 16 (size of the pointer)
    test::checkReturns(0, R"(
struct Node {
    var data: [int, 3];
}
fn main() {
    let root = unique Node();
    return root.data[0];
})");
}

TEST_CASE("Array slice", "[end-to-end][regression]") {
    CHECK(test::compiles(R"(
public fn foo(p: *[int]) -> *[int] { return &p[1 : 2]; }
)"));
}

TEST_CASE("Pointer dereference", "[end-to-end][regression]") {
    CHECK(test::compiles(R"(
fn bar(data: &[bool]) {}
public struct Sieve {
    fn foo(&this) {
        bar(*this.flags);
    }

    var flags: *[bool];
}
)"));
}

TEST_CASE("Move from unique pointer data member", "[end-to-end][regression]") {
    CHECK(test::compiles(R"(
public struct Foo {
    fn bar(rhs: &mut Foo) {
        let p = move rhs.buf;
    }
    var buf: *unique mut str;
})"));
}
