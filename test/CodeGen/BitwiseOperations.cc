#include <Catch/Catch2.hpp>

#include <string>

#include "test/CodeGen/BasicCompiler.h"
#include "VM/Program.h"
#include "VM/VirtualMachine.h"

using namespace scatha;

TEST_CASE("Bitwise left shift") {
	std::string const text = R"(
fn main() -> int {
	return 18964514 << 4;
})";
	auto const registers = test::getRegisters(text);
	CHECK(registers[0] == 303432224);
}

TEST_CASE("Bitwise right shift") {
	std::string const text = R"(
fn main() -> int {
 return 18964514 >> 6;
})";
	auto const registers = test::getRegisters(text);
	CHECK(registers[0] == 296320);
}
