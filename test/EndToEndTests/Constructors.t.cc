#include <Catch/Catch2.hpp>

#include <string>

#include "test/EndToEndTests/PassTesting.h"

using namespace scatha;

/// Since we don't have libraries or multi file compilation we just paste the
/// code here
std::string const CommonDefs = R"(
struct X {
    fn new(&mut this) {
        this.value = 0;
        print("+");
        print(this.value);
    }

    fn new(&mut this, n: int) {
        this.value = n;
        print("+");
        print(this.value);
    }

    fn new(&mut this, rhs: &X) {
        this.value = rhs.value + 1;
        print("+");
        print(this.value);
    }

    fn delete(&mut this) {
        print("-");
        print(this.value);
        this.value = -1;
    }

    var value: int;
}

fn print(text: &str) {
    __builtin_putstr(text);
}

fn print(n: int) {
    __builtin_puti64(n);
})";

TEST_CASE("Constructors", "[end-to-end][constructors]") {
    SECTION("Variables declarations") {
        test::checkPrints("+0-0", CommonDefs + R"(
            fn main() {
                var x: X;
            })");
        test::checkPrints("+0-0", CommonDefs + R"(
            fn main() {
                var x = X();
            })");
        test::checkPrints("+0-0", CommonDefs + R"(
            fn main() {
                var x = X();
                return; // We had an issue where explicit returns would
                        // prevent destructors being called
            })");
        test::checkPrints("+2+3-3-2", CommonDefs + R"(
            fn main() {
                var x = X(2);
                var y = x;
            })");
        test::checkPrints("+0+0-0+2-2-0", CommonDefs + R"(
            fn main() {
                var x = X(X().value);
                var y = X(2);
            })");
        test::checkPrints("+1-1+1-1+1-1", CommonDefs + R"(
            fn main() {
                for i = 1; i <= 3; i += X(1).value {}
            })");
        test::checkPrints("+3-3+3-3+3-3+3-3", CommonDefs + R"(
            fn main() {
                for i = 1; i <= X(3).value; ++i {}
            })");
        test::checkPrints("+1-4", CommonDefs + R"(
            fn main() {
                for x = X(1); x.value <= 3; ++x.value {}
            })");
        test::checkPrints("+0+1-1-0", CommonDefs + R"(
            fn takeCopy(value: X) {}
            fn main() {
                var x = X();
                takeCopy(x);
            })");
        test::checkPrints("+0+1-1-0", CommonDefs + R"(
            fn makeCopy(value: &X) -> X { return value; }
            fn main() {
                var x = X();
                makeCopy(x);
            })");
        test::checkPrints("+0-0", CommonDefs + R"(
            fn takeRef(value: &X) {}
            fn main() {
                var x = X();
                takeRef(x);
            })");
        /// The caller is responsible for destroying by-value arguments, so the
        /// argument is destroyed after the return value
        test::checkPrints("+0+1+2-2-1-0", CommonDefs + R"(
            fn passCopy(value: X) -> X { return value; }
            fn main() {
                var x = X();
                passCopy(x);
            })");
        /// We store the return value in a variable so it is destroyed at scope
        /// exit
        test::checkPrints("+0+1+2-1-2-0", CommonDefs + R"(
            fn passCopy(value: X) -> X { return value; }
            fn main() {
                var x = X();
                let y = passCopy(x);
            })");
        test::checkPrints("+0+1+2-2-1-0", CommonDefs + R"(
            fn main() {
                X(X(X()));
            })");

        /// Assignments
        test::checkPrints("+0-0+0-0", CommonDefs + R"(
            fn main() {
                var x = X();
                x = X();
            })");
        test::checkPrints("+0+1-0+2-1-2", CommonDefs + R"(
            fn main() {
                var x = X(0);
                var y = X(1);
                x = y;
            })");
        test::checkPrints("+0-0", CommonDefs + R"(
            fn main() {
                var x = X();
                x = x;
            })");
        test::checkPrints("+0-0", CommonDefs + R"(
            fn assign(lhs: &mut X, rhs: &X) {
                lhs = rhs;
            }
            fn main() {
                var x = X();
                assign(x, x);
            })");
        test::checkPrints("+0+0-0+1-0-1", CommonDefs + R"(
            fn assign(lhs: &mut X, rhs: &X) {
                lhs = rhs;
            }
            fn main() {
                var x = X();
                assign(x, X());
            })");
        test::checkReturns(8, R"(
        struct X {
            fn new(&mut this) { this.value = 8; }
            fn new(&mut this, rhs: &X) { this.value = rhs.value; }
            fn delete(&mut this) { this.value = -1; }
            var value: int;
        }
        fn pass(x: &X) -> &X { return x; }
        fn main() {
            var x = X();
            x = pass(X());
            return x.value;
        })");
    }
}

TEST_CASE("Pseudo constructors", "[end-to-end][constructors]") {
    test::checkReturns(5, R"(
struct X {
    var i: int;
    var f: float;
    struct Y {
        var k: int;
        var b: byte;
    }
    var y: Y;
}
fn main() -> int {
    let x = X(2, 1.0, X.Y(1, 1));
    return x.i + int(x.f) + x.y.k + int(x.y.b);
})");
}

TEST_CASE("Generated constructors", "[end-to-end][constructors]") {
    auto text = CommonDefs + R"(
struct Z {
    fn new(&mut this) { this.n = 3; }
    var n: int;
}
struct Y {
    var n: int;
    var x: X;
    var z: Z;
}
fn main() {
    var x = Y();
    x.n = 1;
    var y = x;
    return x.z.n + y.z.n + y.n;
})";
    test::checkReturns(7, text);
    test::checkPrints("+0+1-1-0", text);
}

TEST_CASE("Don't pop destructors in reference variables",
          "[end-to-end][constructors]") {
    test::checkPrints("+4+7-7-4", CommonDefs + R"(
fn main() {
    var x = X(4);
    var ref: &X = (X(7).value, x);
})");
}

TEST_CASE("Array default constructor", "[end-to-end][constructors]") {
    test::checkPrints("+0+0+0-0-0-0", CommonDefs + R"(
fn main() {
    var a: [X, 3];
})");
}

TEST_CASE("Array copy constructor", "[end-to-end][constructors]") {
    test::checkPrints("+0+0+1+1-1-1-0-0", CommonDefs + R"(
fn main() {
    var a: [X, 2];
    var b = a;
})");
}

/// FIXME: This fails
/// This fails because there is still a major problem regarding lifetimes:
/// __ Non-trivial lifetime does not imply that we don't have compiler generated
/// constructors __ A type can very well not have trivial lifetime, but still be
/// a "POD-type", for example `struct X { var mem: NonTrivial; };` or
/// `[NonTrivial, 2]`
TEST_CASE("Copy array to function", "[end-to-end][constructors]") {
    return;
    test::checkPrints("+0+0+1+1-1-1-0-0", CommonDefs + R"(
fn f(data: [X, 2]) {}
fn main() {
    var a: [X, 2];
    f(a);
})");
}

TEST_CASE("List expression of non-trivial type", "[end-to-end][constructors]") {
    test::checkPrints("+1+2-1-2", CommonDefs + R"(
fn main() {
    var data = [X(1), X(2)];
})");
}

TEST_CASE("List expression of trivial type", "[end-to-end][constructors]") {
    test::checkPrints("+1+2", R"(
struct Y {
    fn new(&mut this, n: int) {
        __builtin_putstr("+");
        __builtin_puti64(n);
    }
}
fn main() {
    var data = [Y(1), Y(2)];
})");
}

TEST_CASE("First move constructor", "[end-to-end][constructors]") {
    test::checkReturns(10, R"(
struct UniquePtr {
    fn new(&mut this) { this.ptr = null; }
    fn new(&mut this, ptr: *mut int) { this.ptr = ptr; }
    fn move(&mut this, rhs: &mut UniquePtr) {
        this.ptr = rhs.ptr;
        rhs.ptr = null;
    }
    fn delete(&mut this) {
        this.reset();
    }
    fn reset(&mut this) {
        if this.ptr == null {
            return;
        }
        let bytePtr = reinterpret<*mut [byte]>(this.ptr);
        __builtin_dealloc(bytePtr, 8);
        this.ptr = null;
    }
    fn get(&this) { return this.ptr; }
    var ptr: *mut int;
}

fn allocate() -> UniquePtr {
    let ptr = __builtin_alloc(8, 8);
    return UniquePtr(reinterpret<*mut int>(ptr));
}

fn main() {
    var p = allocate();
    let q = move p;
    *q.get() = 10;
    return *q.get();
})");
}
