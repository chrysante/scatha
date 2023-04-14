#include <Catch/Catch2.hpp>

#include <string>

#include "test/EndToEndTests/BasicCompiler.h"

using namespace scatha;

TEST_CASE("fcmp greater var-lit", "[end-to-end]") {
    test::checkReturns(1, R"(
public fn main() -> int {
    let a = 32.1;
    if a > 12.2 {
        return 1;
    }
    else {
        return 2;
    }
})");
}
TEST_CASE("fcmp greater lit-var", "[end-to-end]") {
    test::checkReturns(1, R"(
public fn main() -> int {
    let a = 32.1;
    if 100.0 > a {
        return 1;
    }
    else {
        return 2;
    }
})");
}
TEST_CASE("fcmp less var-lit", "[end-to-end]") {
    test::checkReturns(1, R"(
public fn main() -> int {
    let a = 32.1;
    if a < 112.2 {
        return 1;
    }
    else {
        return 2;
    }
})");
}
TEST_CASE("fcmp less lit-var", "[end-to-end]") {
    test::checkReturns(1, R"(
public fn main() -> int {
    let a = 32.1;
    if -1002.0 < a {
        return 1;
    }
    else {
        return 2;
    }
})");
}
TEST_CASE("fcmp less lit-lit", "[end-to-end]") {
    test::checkReturns(1, R"(
public fn main() -> int {
    let a = 32.1;
    if -1002.0 < 0.0 {
        return 1;
    }
    else {
        return 2;
    }
})");
}
TEST_CASE("nested if-else-if", "[end-to-end]") {
    test::checkReturns(1, R"(
public fn main() -> int {
    let x = 0;
    if -1002.0 > 0.0 {
        return 0;
    }
    else if 1002.0 < 0.0 {
        return 0;
    }
    else if -1 < x {
        return 1;
    }
    else {
        return 2;
    }
})");
}
TEST_CASE("more nested if else", "[end-to-end]") {
    test::checkReturns(1, R"(
public fn main() -> int {
    let x = 0;
    if -1002.0 > 0.0 {
        x = 0;
    }
    else {
        x = 1;
    }
    // just to throw some more complexity at the compiler
    let y = 1 + 2 * 3 / 4 % 5 / 6;
    if x == 1 {
        return x;
    }
    else {
        return x + 100;
    }
})");
}

TEST_CASE("logical not", "[end-to-end]") {
    test::checkReturns(1, R"(
public fn main() -> bool {
    return !false;
})");
}

TEST_CASE("Branch based on literals", "[end-to-end]") {
    test::checkReturns(1, R"(
public fn main() -> int {
    if true {
        return 1;
    }
    else {
        return 0;
    }
})");
}

TEST_CASE("Branch based on result of function calls", "[end-to-end]") {
    test::checkReturns(2, R"(
public fn main() -> int {
    let x = 0;
    let y = 1;
    if greaterZero(x) {
        return 1;
    }
    else if greaterZero(y) {
        return 2;
    }
    else {
        return 3;
    }
}
fn greaterZero(a: int) -> bool {
    return !(a <= 0);
})");
}

TEST_CASE("Conditional", "[end-to-end]") {
    test::checkReturns(2, R"(
public fn main() -> int {
    let x = 0;
    return greaterZero(x) ? 1 : 2;
}
fn greaterZero(a: int) -> bool {
    return !(a <= 0);
})");
}

TEST_CASE("Right-nested conditional", "[end-to-end]") {
    test::checkReturns(2, R"(
public fn main() -> int {
    let x = 0;
    let y = 1;
    return greaterZero(x) ? 1 : greaterZero(y) ? 2 : 3;
}
fn greaterZero(a: int) -> bool {
    return !(a <= 0);
})");
}

TEST_CASE("Left-nested conditional", "[end-to-end]") {
    test::checkReturns(1, R"(
public fn main() -> int {
    let x = 0;
    let y = 1;
    return greaterZero(x + 1) ? greaterZero(y) ? 1 : 2 : 3;
}
fn greaterZero(a: int) -> bool {
    return !(a <= 0);
})");
}

TEST_CASE("Left-nested conditional with literals", "[end-to-end]") {
    test::checkReturns(1, R"(
public fn main() -> int {
    return true ? true ? 1 : 2 : 3;
})");
}
