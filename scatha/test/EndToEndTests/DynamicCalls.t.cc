#include <catch2/catch_test_macros.hpp>

#include "EndToEndTests/PassTesting.h"

using namespace scatha;

TEST_CASE("First dynamic call", "[end-to-end][dyn-calls]") {
    test::runReturnsTest(42, R"(
protocol P { fn test(&this) -> int; }
struct S: P {
    fn test(&dyn this) -> int { return this.value; }
    var value: int;
}
fn f(p: &dyn mut P) -> int {
    return p.test();
}
fn main() {
    var s = S(42);
    return f(s);
}
)");
}

TEST_CASE("Multiple inheritance with protocols", "[end-to-end][dyn-calls]") {
    auto* expected =
        R"(Base1(7) -> h
Derived2(11).h
Base2(42) -> g
Derived2(11).g
)";
    test::runPrintsTest(expected, { R"(
protocol Foo {
    fn f(&this) -> void;
    fn g(&this) -> void;
}
struct Base1: Foo {
    fn f(&dyn this) {
        __builtin_putstr("Base1(");
        __builtin_puti64(this.value);
        __builtin_putstr(") -> h\n");
        this.h();
    }
    fn h(&dyn this) {}
    var value: int;
}
struct Base2: Foo {
    fn f(&dyn this) {
        __builtin_putstr("Base2(");
        __builtin_puti64(this.value);
        __builtin_putstr(") -> g\n");
        this.g();
    }
    var value: int;
}
struct Derived: Base1, Base2 {
    fn g(&dyn this) {
        __builtin_putstr("Derived2(");
        __builtin_puti64(this.value);
        __builtin_putstr(").g\n");
    }
    fn h(&dyn this) {
        __builtin_putstr("Derived2(");
        __builtin_puti64(this.value);
        __builtin_putstr(").h\n");
    }
    var value: int;
}
fn main() {
    let d = Derived(Base1(7), Base2(42), 11);
    let b1foo: &dyn Foo = d as &dyn Base1;
    b1foo.f();
    let b2foo: &dyn Foo = d as &dyn Base2;
    b2foo.f();
})" });
}

TEST_CASE("Forward calls up the inheritance hierarchy",
          "[end-to-end][dyn-calls]") {
    test::runPrintsTest("B.a -> C.c -> D.c -> 42", { R"(
struct Offset {
    fn placeholder(&dyn this) { __builtin_putstr("placeholder"); }
}
struct A {
    fn a(&dyn this) -> int {
        __builtin_putstr("A.a -> ");
        return 0;
    }
}
struct B: Offset, A {
    fn a(&dyn this) {
        __builtin_putstr("B.a -> ");
        return this.b();
    }
    fn b(&dyn this) {
        __builtin_putstr("B.b -> ");
        return 0;
    }
}
struct C: Offset, B {
    fn b(&dyn this) {
        __builtin_putstr("C.c -> ");
        return this.c();
    }
    fn c(&dyn this) {
        __builtin_putstr("C.c -> ");
        return 0;
    }
}
struct D: Offset, C {
    fn new(&mut this, value: int) {
        this.value = value;
    }
    fn c(&dyn this) {
        __builtin_putstr("D.c -> ");
        return this.value;
    }
    var value: int;
}
fn main() {
    let d = D(42);
    let a: &dyn A = d;
    __builtin_puti64(a.a());
})" });
}

TEST_CASE("Mutually recursive dynamic call", "[end-to-end][dyn-calls]") {
    test::runReturnsTest(1024, R"(
struct X {
    fn f(&dyn mut this) -> int {
        return this.g();
    }
    fn g(&dyn mut this) -> int {
        return 0;
    }
}
struct Y: X {
    fn g(&dyn mut this) -> int {
        if this.i++ < 10 {
            return 2 * this.f();
        }
        return 1;
    }
    var i: int;
}
fn main() -> int {
    var y = Y();
    return y.f();
}
)");
}
