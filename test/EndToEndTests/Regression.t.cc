#include <Catch/Catch2.hpp>

#include "test/EndToEndTests/PassTesting.h"

using namespace scatha;

TEST_CASE("CodeGen DCE wrongly eliminates function calls with side effects",
          "[end-to-end][regression]") {
    test::checkReturns(10, R"(
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
    test::checkReturns(1, R"(
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
    /// road Unfortunately I was not able to simplify the code sample any
    /// further while still reproducing the issue.
    test::checkCompiles(R"(
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
})");
}

TEST_CASE("Bug in LoopRotate - 1", "[end-to-end][regression]") {
    test::checkReturns(3, R"(
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
    test::checkReturns(4, R"(
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
    test::checkReturns(10, R"(
fn main(n: int, cond: bool) -> int {
    if cond {}
    else {}
    n ^= n;
    n += 10;
    return n;
})");
}

TEST_CASE("Size of array data member", "[end-to-end][regression]") {
    test::checkReturns(5, R"(
struct X {
    var data: [int, 5];
}
fn main(cond: bool) -> int {
    var x: X;
    return x.data.count;
})");
}

TEST_CASE("Pass large array by value", "[end-to-end][regression]") {
    test::checkReturns(1, R"(
fn first(data: [int, 10]) -> int {
    return data[0];
}
fn main() -> int {
    let data = [1, 2, 3, 4, 5, 6, 7, 8, 9, 0];
    return first(data);
})");
}

TEST_CASE("MemToReg bug") {
    test::checkIRReturns(42, R"(
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
    test::checkIRReturns(6, R"(
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
    test::checkReturns(3, R"(
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
    test::checkIRReturns(3, R"(
func i64 @main() {
%entry:
    %0 = scmp eq i32 0, i32 1
    %1 = scmp eq i32 1, i32 2
    
    %s = select i1 %0, i64 1, i64 2
    %r = select i1 %1, i64 %s, i64 3
    return i64 %r
})");
}
