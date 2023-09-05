#include <Catch/Catch2.hpp>

#include "test/EndToEndTests/PassTesting.h"

using namespace scatha;

TEST_CASE("CodeGen DCE wrongly eliminates function calls with side effects",
          "[end-to-end][regression]") {
    test::checkReturns(10, R"(
public fn main() -> int {
    var i = 0;
    modifyWithIgnoredReturnValue(&i);
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
public fn main() -> int {
    var a = [0, 0];
    var b = [1, 2];
    f(&a, &b);
    return a[0];
})");
}

TEST_CASE("Weird bug in simplifyCFG", "[end-to-end][regression]") {
    /// `simplifyCFG()`would crash because an erased basic block was still in
    /// the worklist, so the algorithm would read deallocated memory down the
    /// road Unfortunately I was not able to simplify the code sample any
    /// further while still reproducing the issue.
    test::checkCompiles(R"(
public fn main() {
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
    __builtin_putstr(&msg);
})");
}

TEST_CASE("Bug in LoopRotate - 1", "[end-to-end][regression]") {
    test::checkReturns(3, R"(
public fn main() -> int {
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
public fn main() -> int {
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
public fn main(n: int, cond: bool) -> int {
    if cond {}
    else {}
    n ^= n;
    n += 10;
    return n;
})");
}
