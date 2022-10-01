#include <Catch/Catch2.hpp>

#include <string>

#include "test/CodeGen/BasicCompiler.h"
#include "VM/Program.h"
#include "VM/VirtualMachine.h"

using namespace scatha;

TEST_CASE("Overloading", "[codegen]") {
	std::string const text = R"(

fn add(f: float) -> int { return 1; }

fn add(x: int) -> int {
	return x;
}

fn add(x: int, y: int) -> int {
	return x + y;
}

fn add(x: int, y: int, z: int) -> int {
	return x + y + z;
}

fn main() -> int {
	return add(1.0) * add(1) + add(2, 3) + add(4, 5, 6);
}

)";
	
	auto const registers = test::getRegisters(text);
	CHECK(registers[0] == 1 + 2 + 3 + 4 + 5 + 6);
}

TEST_CASE("Overloading 2", "[codegen]") {
	std::string const text = R"(

fn f(i: int, b: bool) -> int { return 1; }
fn f(i: float, b: bool) -> int { return 2; }
fn f(i: bool, b: bool) -> int { return 3; }

fn main() -> int {
	return f(0.0, true);
}

)";
	
	auto const registers = test::getRegisters(text);
	CHECK(registers[0] == 2);
}
