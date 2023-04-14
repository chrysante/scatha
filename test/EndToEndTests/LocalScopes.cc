#include <Catch/Catch2.hpp>

#include <string>

#include "test/EndToEndTests/BasicCompiler.h"

using namespace scatha;

TEST_CASE("Local scopes", "[end-to-end]") {
    test::checkReturns(1, R"(
public fn main() -> int {
    let x = 0;
    {
        // local name x shadows outer x
        let x = 1;
        return x;
    }
})");
}

TEST_CASE("Local scopes 2", "[end-to-end]") {
    test::checkReturns(2, R"(
public fn main() -> int {
    let x = 0;
    {
        // local name x shadows outer x
        let x = 1;
    } {
        /* and again */
        let x = 2;
        return x;
    }
})");
}
