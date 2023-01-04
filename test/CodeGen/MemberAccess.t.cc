#include <Catch/Catch2.hpp>

#include <string>

#include "VM/Program.h"
#include "VM/VirtualMachine.h"
#include "test/CodeGen/BasicCompiler.h"

using namespace scatha;

TEST_CASE("Member access", "[codegen]") {
    std::string const text = R"(
struct X {
    var anInteger: int;
    var aFloat: float;
    var aSecondInt: int;
}
struct Y {
    var i: int;
    var x: X;
}
fn main() -> int {
    var y: Y;
    y.x.aSecondInt = 4;
    return y.x.aSecondInt;
})";
    auto const vm          = test::compileAndExecute(text);
    auto const& state      = vm.getState();
    CHECK(state.registers[0] == 4);
}


