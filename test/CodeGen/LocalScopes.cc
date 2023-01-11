#include <Catch/Catch2.hpp>

#include <string>

#include "VM/Program.h"
#include "VM/VirtualMachine.h"
#include "test/CodeGen/BasicCompiler.h"

using namespace scatha;

TEST_CASE("Local scopes", "[codegen]") {
    test::checkReturns(1, R"(
fn main() -> int {
    let x = 0;
    {
        // local name x shadows outer x
        let x = 1;
        return x;
    }
})");
}

TEST_CASE("Local scopes 2", "[codegen]") {
    test::checkReturns(2, R"(
fn main() -> int {
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
