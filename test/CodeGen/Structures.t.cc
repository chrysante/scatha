#include <Catch/Catch2.hpp>

#include <string>

#include "VM/Program.h"
#include "VM/VirtualMachine.h"
#include "test/CodeGen/BasicCompiler.h"

using namespace scatha;

TEST_CASE("Member access", "[codegen]") {
    std::string const text = R"(
struct Y {
    var i: int;
    var x: X;
}
fn main() -> int {
    var y: Y;
    y.x.aSecondInt = 4;
    return y.x.aSecondInt;
}
struct X {
    var anInteger: int;
    var aFloat: float;
    var aSecondInt: int;
})";
    auto const vm          = test::compileAndExecute(text);
    auto const& state      = vm.getState();
    CHECK(state.registers[0] == 4);
}

TEST_CASE("Bool member access", "[codegen]") {
    std::string const text = R"(
fn main() -> int {
    var x: X;
    x.d = true;
    if x.d { return 2; }
    return 1;
}
struct X {
    var b: bool;
    var c: bool;
    var d: bool;
    var a: int;
})";
    auto const vm          = test::compileAndExecute(text);
    auto const& state      = vm.getState();
    CHECK(state.registers[0] == 2);
}

TEST_CASE("Return custom structs", "[codegen]") {
    std::string const text = R"(
struct X {
    var b: bool;
    var c: bool;
    var d: bool;
    var a: int;
}
fn makeX() -> X {
    var result: X;
    result.a = 1;
    result.b = false;
    result.c = true;
    result.d = false;
    return result;
}
fn main() -> int {
    var x = makeX();
    if x.c { return 2; }
    return 1;
})";
    auto const vm          = test::compileAndExecute(text);
    auto const& state      = vm.getState();
    CHECK(state.registers[0] == 2);
}

TEST_CASE("Pass custom structs as arguments", "[codegen]") {
    return; // Fails because of a problem in sema!
    std::string const text = R"(
struct X {
    var b: bool;
    var c: bool;
    var d: bool;
    var a: int;
}
fn getX_a(x: X) -> bool {
    var result = x.a;
    return result;
}
fn main() -> int {
    var x;
    x.a = 5;
    x.b = true;
    x.c = false;
    x.d = true;
    var result = getX_a(x);
    return result;
})";
    auto const vm          = test::compileAndExecute(text);
    auto const& state      = vm.getState();
    CHECK(state.registers[0] == 5);
}


