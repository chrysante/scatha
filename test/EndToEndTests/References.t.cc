#include <Catch/Catch2.hpp>

#include "test/EndToEndTests/BasicCompiler.h"

using namespace scatha;

TEST_CASE("First reference parameter", "[end-to-end][references]") {
    test::checkReturns(4, R"(
public fn main() -> int {
    var i = 3;
    f(&i);
    return i;
}
fn f(x: &mut int)  {
    x += 1;
})");
}

TEST_CASE("Rebind reference", "[end-to-end][references]") {
    test::checkReturns(2, R"(
public fn main() -> int {
    var i = 0;
    var j = 0;
    var r = &mut i;
    r += 1;
    r = &mut j;
    r += 1;
    return i + j;
})");
}

TEST_CASE("Pass reference through function", "[end-to-end][references]") {
    test::checkReturns(1, R"(
public fn main() -> int {
    var i = 0;
    var j: &mut int = &f(&i);
    j = 1;
    return i;
}
fn f(x: &mut int)  -> &mut int {
    return &x;
})");
}

TEST_CASE("First array", "[end-to-end][arrays]") {
    test::checkReturns(2, R"(
public fn main() -> int {
    var arr: [int, 4] = [1, 2, 3, 4];
    return arr[1];
})");
}

TEST_CASE("Reference to array element", "[end-to-end][arrays]") {
    test::checkReturns(5, R"(
public fn main() -> int {
    var arr = [1, 2, 3, 4];
    var r = &arr[1];
    r = 5;
    return arr[1];
})");
}
