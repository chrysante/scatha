#include <catch2/catch_test_macros.hpp>

#include "EndToEndTests/PassTesting.h"

using namespace scatha;

TEST_CASE("Simple fstring", "[end-to-end][fstring]") {
    test::runPrintsTest("The answer is: 42!\nOne half is 0.5\n", { R"TEXT(
fn main() {
    let i = 42;
    __builtin_putln("The answer is: \(i)!" as *);
    __builtin_putln("One half is \(1. / 2.)" as *);
})TEXT" });
}
