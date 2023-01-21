#include <Catch/Catch2.hpp>

#include <string>

#include "test/CodeGen/BasicCompiler.h"

using namespace scatha;

TEST_CASE("While loop", "[codegen]") {
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
fn main() -> int {
    return fact(4);
})");
}

TEST_CASE("Iterative gcd", "[codegen]") {
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
fn main() -> int {
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
fn main() -> int {
    let a = 756476;
    let b = 1253;
    return gcd(a, b) + gcd(1, 7);
})");
}

TEST_CASE("Float pow", "[codegen]") {
    test::checkReturns(1, R"(
fn pow(base: float, exp: int) -> float {
    var result: float = 1.0;
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
fn main() -> bool {
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

TEST_CASE("For loop", "[codegen]") {
    test::checkReturns(24, R"(
fn fact(n: int) -> int {
    var result = 1;
    for i = 1; i <= n; i += 1 {
        result *= i;
    }
    return result;
}
fn main() -> int {
    return fact(4);
})");
}

TEST_CASE("Float pow / for", "[codegen]") {
    test::checkReturns(1, R"(
fn pow(base: float, exp: int) -> float {
    var result: float = 1.0;
    if (exp < 0) {
        base = 1.0 / base;
        exp = -exp;
    }
    for i = 0; i < exp; i += 1 {
        result *= base;
    }
    return result;
}
fn main() -> bool {
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

TEST_CASE("Do/while loop", "[codegen]") {
    test::checkReturns(24, R"(
fn fact(n: int) -> int {
    var result = 1;
    var i = 0;
    do {
        i += 1;
        result *= i;
    } while i < n;
    return result;
}
fn main() -> int {
    return fact(4);
})");
}

TEST_CASE("Nested loops", "[codegen]") {
    test::checkReturns(2 * 3, R"(
fn main() -> int {
    var acc = 0;
    for j = 0; j < 2; j += 1 {
        for i = 0; i < 3; i += 1 {
            acc += 1;
        }
    }
    return acc;
})");
    test::checkReturns(2 * 3 * 4, R"(
fn main() -> int {
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
fn main() -> int {
    var acc = 0;
    for k = 0; k < 2; k += 1 {
        for j = 0; j < 3; j += 1 {
            var i = 0;
            while i < 4 {
                acc += 1;
                i += 1;
            }
        }
    }
    return acc;
})");
    test::checkReturns(2 * 3 * 4, R"(
fn main() -> int {
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

TEST_CASE("Load of indirectly stored struct", "[codegen]") {
    test::checkReturns(10, R"(
fn main() -> int {
    var acc = 0;
    for i = 0; i < 5; i += 1 {
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
