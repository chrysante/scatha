#include <Catch/Catch2.hpp>

#include "test/EndToEndTests/PassTesting.h"

using namespace scatha;

TEST_CASE("CodeGen DCE wrongly eliminates function calls with side effects",
          "[end-to-end][regression]") {
    test::checkReturns(10, R"(
public fn main() -> int {
    var i = 0;
    modifyWithIgnoredReturnValue(&mut i);
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
    f(&mut a, &b);
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
