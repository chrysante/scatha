#include <Catch/Catch2.hpp>

#include <string>

#include "test/EndToEndTests/PassTesting.h"

using namespace scatha;

TEST_CASE("While loop", "[end-to-end]") {
    test::checkReturns(24, R"(
fn fact(n: int) -> int {
    var i = 0;
    var result = 1;
    while i < n {
        i += 1;
        result *= i;
    }
    return result;
}
public fn main() -> int {
    return fact(4);
})");
}

TEST_CASE("Iterative gcd", "[end-to-end]") {
    test::checkReturns(7, R"(
fn gcd(a: int, b: int) -> int {
    while a != b {
        if a > b {
            a -= b;
        }
        else {
            b -= a;
        }
    }
    return a;
}
public fn main() -> int {
    let a = 756476;
    let b = 1253;
    return gcd(a, b);
})");
    test::checkReturns(8, R"(
fn gcd(a: int, b: int) -> int {
    while b != 0 && true {
        let t = b + 0;
        b = a % b;
        a = t;
    }
    return a;
}
public fn main() -> int {
    let a = 756476;
    let b = 1253;
    return gcd(a, b) + gcd(1, 7);
})");
}

TEST_CASE("Float pow", "[end-to-end]") {
    test::checkReturns(1, R"(
fn pow(base: double, exp: int) -> double {
    var result: double = 1.0;
    var i = 0;
    if (exp < 0) {
        base = 1.0 / base;
        exp = -exp;
    }
    while i < exp {
        result *= base;
        i += 1;
    }
    return result;
}
public fn main() -> bool {
    var result = true;
    result &= pow( 0.5,  3) == 0.125;
    result &= pow( 1.5,  3) == 1.5 * 2.25;
    result &= pow( 1.0, 10) == 1.0;
    result &= pow( 2.0, 10) == 1024.0;
    result &= pow( 2.0, -3) == 0.125;
    result &= pow(-2.0,  9) == -512.0;
    return result;
})");
}

TEST_CASE("For loop", "[end-to-end]") {
    test::checkReturns(24, R"(
fn fact(n: int) -> int {
    var result = 1;
    for i = 1; i <= n; i += 1 {
        result *= i;
    }
    return result;
}
public fn main() -> int {
    return fact(4);
})");
}

TEST_CASE("Float pow / for", "[end-to-end]") {
    test::checkReturns(1, R"(
fn pow(base: double, exp: int) -> double {
    var result: double = 1.0;
    if (exp < 0) {
        base = 1.0 / base;
        exp = -exp;
    }
    for i = 0; i < exp; ++i {
        result *= base;
    }
    return result;
}
public fn main() -> bool {
    var result = true;
    result &= pow( 0.5,  3) == 0.125;
    result &= pow( 1.5,  3) == 1.5 * 2.25;
    result &= pow( 1.0, 10) == 1.0;
    result &= pow( 2.0, 10) == 1024.0;
    result &= pow( 2.0, -3) == 0.125;
    result &= pow(-2.0,  9) == -512.0;
    return result == true;
})");
}

TEST_CASE("Do/while loop", "[end-to-end]") {
    test::checkReturns(24, R"(
fn fact(n: int) -> int {
    var result = 1;
    var i = n;
    do {
        result *= i;
        --i;
    } while i > 0;
    return result;
}
public fn main() -> int {
    return fact(4);
})");
}

TEST_CASE("Nested loops", "[end-to-end]") {
    test::checkReturns(2 * 3, R"(
public fn main() -> int {
    var acc = 0;
    for j = 0; j < 2; ++j {
        for i = 0; i < 3; ++i {
            ++acc;
        }
    }
    return acc;
})");
    test::checkReturns(2 * 3 * 4, R"(
public fn main() -> int {
    var acc = 0;
    for k = 0; k < 2; k += 1 {
        for j = 0; j < 3; j += 1 {
            for i = 0; i < 4; i += 1 {
                acc += 1;
            }
        }
    }
    return acc;
})");
    test::checkReturns(2 * 3 * 4, R"(
public fn main() -> int {
    var acc = 0;
    for k = 0; k < 2; k += 1 {
        for j = 0; j < 3; j += 1 {
            var i = 0;
            while i < 4 {
                acc += 1;
                ++i;
            }
        }
    }
    return acc;
})");
    test::checkReturns(2 * 3 * 4, R"(
public fn main() -> int {
    var acc = 0;
    for k = 0; k < 2; k += 1 {
        for j = 0; j < 3; j += 1 {
            var i = 0;
            do {
                acc += 1;
                i += 1;
            } while i < 4;
        }
    }
    return acc;
})");
}

TEST_CASE("Load of indirectly stored struct", "[end-to-end]") {
    test::checkReturns(10, R"(
public fn main() -> int {
    var acc = 0;
    for i = 0; i < 5; ++i {
        var z: Complex;
        z.x = 0;
        z.y = i;
        acc += sum(z);
    }
    return acc;
}
fn sum(z: Complex) -> int { return z.x + z.y; }
struct Complex {
    var x: int;
    var y: int;
})");
}

TEST_CASE("For loop with nested if/else", "[end-to-end]") {
    test::checkReturns(48, R"(
    fn g(n: int) -> int {
        var result = 1;
        for i = 1; i <= n; ++i {
            if i % 2 == 0 {
                result *= i;
            }
        }
        return result;
    }
    public fn main() -> int { return g(6); })");
}

TEST_CASE("For loop with break and continue", "[end-to-end]") {
    test::checkReturns(36, R"(
public fn main() -> int {
    return g(20);
}
fn g(n: int) -> int {
    var result = 0;
    for i = 0; i < n; ++i {
        if i % 2 == 0 {
            continue;
        }
        result += i;
        if (i >= 10) {
            break;
        }
    }
    return result;
})");
}

TEST_CASE("While loop with break and continue", "[end-to-end]") {
    test::checkReturns(36, R"(
public fn main() -> int {
    return g(20);
}
fn g(n: int) -> int {
    var result = 0;
    var j = 0;
    while j < n {
        let i = j;
        ++j;
        if i % 2 == 0 {
            continue;
        }
        result += i;
        if (i >= 10) {
            break;
        }
    }
    return result;
})");
}

TEST_CASE("Fibonacci ('Double recursion')", "[end-to-end]") {
    test::checkReturns(13, R"(
fn fib(n: int) -> int {
    if n < 3 {
        return 1;
    }
    return fib(n - 1) + fib(n - 2);
}
public fn main() -> int {
    return fib(7);
})");
}

TEST_CASE("Funny loop with two predecessors", "[end-to-end]") {
    test::checkIRReturns(5, R"(
func i64 @main(i1 %0) {
  %entry:
    branch i1 %0, label %if.then, label %if.else

  %if.then:                   # preds: entry
    %a = add i64 5, i64 5
    goto label %loop.header

  %if.else:                   # preds: entry
    %b = add i64 0, i64 10
    goto label %loop.header

  %loop.header:               # preds: if.then, if.else, loop.inc
    %n = phi i64 [label %if.then : %a], [label %if.else : %b], [label %loop.inc : %next]
    %i = phi i64 [label %if.then : 0], [label %if.else : 0], [label %loop.inc : %++.result]
    %cmp.result = scmp ls i64 %i, i64 %n
    branch i1 %cmp.result, label %loop.body, label %loop.end

  %loop.body:                 # preds: loop.header
    %next = sub i64 %n, i64 1
    goto label %loop.inc

  %loop.inc:                  # preds: loop.body
    %++.result = add i64 %i, i64 1
    goto label %loop.header

  %loop.end:                  # preds: loop.header
    return i64 %n
})");
}
