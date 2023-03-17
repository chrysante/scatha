#include <Catch/Catch2.hpp>

#include <string>

#include "test/EndToEndTests/BasicCompiler.h"

using namespace scatha;

TEST_CASE("Member access", "[codegen][member-access]") {
    test::checkReturns(4, R"(
struct Y {
    var i: int;
    var x: X;
}
fn main() -> int {
    var y: Y;
    y.x.aSecondInt = 4;
    return y.x.aSecondInt;
}
struct X {
    var anInteger: int;
    var aFloat: float;
    var aSecondInt: int;
})");
}

TEST_CASE("Bool member access", "[codegen][member-access]") {
    test::checkReturns(2, R"(
fn main() -> int {
    var x: X;
    x.d = true;
    if x.d { return 2; }
    return 1;
}
struct X {
    var b: bool;
    var c: bool;
    var d: bool;
    var a: int;
})");
}

TEST_CASE("Return custom structs", "[codegen][member-access]") {
    test::checkReturns(2, R"(
struct X {
    var b: bool;
    var c: bool;
    var d: bool;
    var a: int;
}
fn makeX() -> X {
    var result: X;
    result.a = 1;
    result.b = false;
    result.c = true;
    result.d = false;
    return result;
}
fn main() -> int {
    var x = makeX();
    if x.c { return 2; }
    return 1;
})");
}

TEST_CASE("Pass custom structs as arguments", "[codegen][member-access]") {
    test::checkReturns(5, R"(
struct X {
    var b: bool;
    var c: bool;
    var d: bool;
    var a: int;
}
fn getX_a(x: X) -> int {
    var result = x.a;
    return result;
}
fn main() -> int {
    var x: X;
    x.a = 5;
    x.b = true;
    x.c = false;
    x.d = true;
    var result = getX_a(x);
    return result;
})");
}

TEST_CASE("Pass and return custom structs and access rvalue",
          "[codegen][member-access]") {
    test::checkReturns(5, R"(
fn main() -> int {
    var x: X;
    x.a = 5;
    x.b = true;
    x.c = false;
    x.d = true;
    var y = forward(x);
    return y.a;
}
fn forward(x: X) -> X {
    return x;
}
struct X {
    var b: bool;
    var c: bool;
    var d: bool;
    var a: int;
})");
}

TEST_CASE("More complex structure passing", "[codegen][member-access]") {
    test::checkReturns(5, R"(
struct X {
    var b: bool;
    var c: bool;
    var d: bool;
    var a: int;
    var y: Y;
}
struct Y {
    var i: int;
    var f: float;
}
fn makeX() -> X {
    var x: X;
    x.a = 5;
    x.b = false;
    x.c = true;
    x.d = false;
    x.y = makeY();
    return forward(x);
}
fn makeY() -> Y {
    var y: Y;
    y.i = -1;
    y.f = 0.5;
    return y;
}
fn forward(x: X) -> X { return x; }
fn forward(y: Y) -> Y { return y; }
fn main() -> int {
    if forward(makeX().y).f == 0.5 {
        return 5;
    }
    return 6;
})");
}

TEST_CASE("Member access mem2reg failure", "[codegen][member-access]") {
#warning Known failure
    return;
    test::checkReturns(1, R"(
fn modifyX(x: X) -> X {
    x.a = 1;
    return x;
}
fn main() -> int {
    var x: X;
    x.a = 0;
    x.b = 0;
    x = modifyX(x);
    return x.a;
}
struct X {
    var a: int;
    var b: int;
})");
}
