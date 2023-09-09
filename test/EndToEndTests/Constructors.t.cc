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
            public fn main() {
                var x: X;
            })");
        test::checkPrints("+0-0", CommonDefs + R"(
            public fn main() {
                var x = X();
            })");
        test::checkPrints("+2+3-3-2", CommonDefs + R"(
            public fn main() {
                var x = X(2);
                var y = x;
            })");
        test::checkPrints("+0+0-0+2-2-0", CommonDefs + R"(
            public fn main() {
                var x = X(X().value);
                var y = X(2);
            })");
        test::checkPrints("+1-1+1-1+1-1", CommonDefs + R"(
            public fn main() {
                for i = 1; i <= 3; i += X(1).value {}
            })");
        test::checkPrints("+3-3+3-3+3-3+3-3", CommonDefs + R"(
            public fn main() {
                for i = 1; i <= X(3).value; ++i {}
            })");
        test::checkPrints("+1-4", CommonDefs + R"(
            public fn main() {
                for x = X(1); x.value <= 3; ++x.value {}
            })");
    }
}
