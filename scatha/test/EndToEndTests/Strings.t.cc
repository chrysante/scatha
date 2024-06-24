#include <catch2/catch_test_macros.hpp>

#include "EndToEndTests/PassTesting.h"

using namespace scatha;

TEST_CASE("Simple fstrings", "[end-to-end][fstring]") {
    SECTION("Format int") {
        test::runPrintsTest("The answer is: 42!\n", { R"TEXT(
fn main() {
    let i = 42;
    __builtin_putln("The answer is: \(i)!" as *);
})TEXT" });
    }
    SECTION("Format narrow int") {
        test::runPrintsTest("The answer is: 42!\n", { R"TEXT(
fn main() {
    let i: s8 = 42;
    __builtin_putln("The answer is: \(i)!" as *);
})TEXT" });
    }
    SECTION("Format double") {
        test::runPrintsTest("One half is 0.5\n", { R"TEXT(
fn main() {
    __builtin_putln("One half is \(1. / 2.)" as *);
})TEXT" });
    }
    SECTION("Format float") {
        test::runPrintsTest("One half is 0.5\n", { R"TEXT(
fn main() {
    __builtin_putln("One half is \(float(1. / 2.))" as *);
})TEXT" });
    }
    SECTION("Format bool") {
        test::runPrintsTest("true and false\n", { R"TEXT(
fn main() {
    __builtin_putln("\(1 == 1) and \(1 == 0)" as *);
})TEXT" });
    }
    SECTION("Format char") {
        test::runPrintsTest("abc\n", { R"TEXT(
fn main() {
    __builtin_putln("\('a')\('b')\('c')" as *);
})TEXT" });
    }
    SECTION("Format str") {
        test::runPrintsTest("Hello World\n", { R"TEXT(
fn main() {
    __builtin_putln("\("Hello World")" as *);
})TEXT" });
    }
    SECTION("Format pointer") {
        CHECK(test::compiles(R"TEXT(
fn main() {
    let n = 42;
    "\(&n)";
    let p = unique [int](10);
    "\(p)";
})TEXT"));
    }
    SECTION("Nesting fstrings") {
        test::runPrintsTest("Hello World\n", { R"TEXT(
fn main() {
    let text = "Hello World";
    __builtin_putln("\("\(text)")" as *);
})TEXT" });
    }
}
