#include <catch2/catch_test_macros.hpp>

#include <string>

#include "EndToEndTests/PassTesting.h"

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
        test::checkPrints("+0+0-0+1-0-1", CommonDefs + R"(
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
        test::runReturnsTest(8, R"(
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
    test::runReturnsTest(5, R"(
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

TEST_CASE("Pseudo constructors zero init", "[end-to-end][constructors]") {
    test::runReturnsTest(true, R"(
struct X {
    var f: double;
    var i: int;
    var p: *int;
}
fn main() -> bool {
    let x: X;
    return x.f == 0.0 && x.i == 0 && x.p == null;
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
    test::runReturnsTest(7, text);
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
    test::runReturnsTest(10, R"(
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

TEST_CASE("Unique ptr to non-trivial type", "[end-to-end][constructors]") {
    SECTION("Construct and destroy") {
        test::checkPrints("+0-0", CommonDefs + R"(
fn main() {
    var p = unique X();
})");
        test::checkPrints("+1-1", CommonDefs + R"(
fn main() {
    var p = unique X(1);
})");
    }
    SECTION("Construct, move destroy") {
        test::checkPrints("+1-1", CommonDefs + R"(
fn main() {
    var p = unique X(1);
    var q = move p;
})");
    }
    SECTION("Pass to function") {
        test::checkPrints("+1-1", CommonDefs + R"(
fn take(p: *unique X) {}
fn main() {
    take(unique X(1));
})");
        test::checkPrints("+1-1", CommonDefs + R"(
fn take(p: *unique X) {}
fn main() {
    var p = unique X(1);
    take(move p);
})");
    }
    SECTION("Return from function") {
        test::checkPrints("+1-1", CommonDefs + R"(
fn give() -> *unique X { return unique X(1); }
fn main() {
    give();
})");
    }
    SECTION("Array of unique pointers") {
        test::checkPrints("+1+2+3-1-2-3", CommonDefs + R"(
fn main() {
    let arr = [unique X(1), unique X(2), unique X(3)];
})");
        test::checkPrints("+1+2+3-1-2-3", CommonDefs + R"(
fn take(arr: [*unique mut X, 3]) {}
fn main() {
    var arr = [unique X(1), unique X(2), unique X(3)];
    take(move arr);
})");
        test::checkPrints("+1+2+3-1-2-3", CommonDefs + R"(
fn give() {
    return [unique X(1), unique X(2), unique X(3)];
}
fn main() {
    give();
})");
        test::checkPrints("+1+2+3-1-2-3", CommonDefs + R"(
fn give() {
    var arr = [unique X(1), unique X(2), unique X(3)];
    return move arr;
}
fn main() {
    give();
})");
    }
    SECTION("Construct and destroy type with unique ptr member") {
        test::checkPrints("+1-1", CommonDefs + R"(
struct P {
    fn new(&mut this, n: int) { this.p = unique X(n); }
    fn delete(&mut this) { } // We have an empty user defined destructor to
                             // test if the unique pointer still gets destroyed
    var p: *unique X;
}
fn main() {
    var p = P(1);
})");
    }
}

TEST_CASE("Unique ptr to dynamic array", "[end-to-end][lib][nativelib]") {
    SECTION("Default construct") {
        test::checkReturns(0, R"(
public fn main() -> int {
    var ptr: *unique [int];
    return ptr.count;
})");
    }
    SECTION("Unique expr") {
        test::checkReturns(5, R"(
         public fn main() {
            let ptr = unique str("12345");
            return ptr.count;
        })");
    }
    SECTION("Unique expr with non-trivial type") {
        test::checkReturns(6, R"(
fn main() {
    let xs = [X(1), X(2), X(3)];
    var ptr = unique [X](xs);
    return ptr[0].value + ptr[1].value + ptr[2].value;
}
struct X {
    fn new(&mut this, n: int) { this.value = n; }
    fn new(&mut this, rhs: &X) { this.value = rhs.value; }
    fn delete(&mut this) {}
    var value: int;
})");
    }
}
