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

TEST_CASE("Bitwise and") {
	std::string const text = R"(
fn main() -> int {
	return 29 & 23;
})";
	auto const registers = test::getRegisters(text);
	CHECK(registers[0] == 21);
}

TEST_CASE("Bitwise and2") {
	std::string const text = R"(
fn main() -> int {
	return (128 -  1) &
		   (128 -  2) &
		   (128 -  4) &
		   (128 -  8) &
		   (128 - 16) &
		   (128 - 32) &
		   (128 - 64);
})";
	auto const registers = test::getRegisters(text);
	CHECK(registers[0] == 64);
}

TEST_CASE("Bitwise or") {
	std::string const text = R"(
fn main() -> int {
	return 29 | 23;
})";
	auto const registers = test::getRegisters(text);
	CHECK(registers[0] == 31);
}

TEST_CASE("Bitwise or2") {
	std::string const text = R"(
fn main() -> int {
	return 1 |
           2 |
           4 |
           8 |
		  16 |
		  32 |
		  64;
})";
	auto const registers = test::getRegisters(text);
	CHECK(registers[0] == 127);
}

TEST_CASE("Bitwise xor") {
	std::string const text = R"(
fn main() -> int {
	return 29 ^ 23;
})";
	auto const registers = test::getRegisters(text);
	CHECK(registers[0] == 10);
}

TEST_CASE("Bitwise not") {
	std::string const text = R"(
fn main() -> int {
	return ~23;
})";
	auto const registers = test::getRegisters(text);
	CHECK(registers[0] == ~u64(23));
}
