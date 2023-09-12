#include <Catch/Catch2.hpp>

#include <string>

#include "test/EndToEndTests/PassTesting.h"

using namespace scatha;

/// Since we don't have libraries or multi file compilation we just paste the
/// code here
static const std::string CommonDefs = R"(
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
    }

    var value: int;
}

fn print(text: &str) {
    __builtin_putstr(text);
}

fn print(n: int) {
    __builtin_puti64(n);
})";

TEST_CASE("Constructors", "[end-to-end][member-access]") {
    SECTION("Variables declarations") {
        test::checkPrints("+0-0", CommonDefs + R"(
            fn main() {
                var x: X;
            })");
        test::checkPrints("+0-0", CommonDefs + R"(
            fn main() {
                var x = X();
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
    }
}
