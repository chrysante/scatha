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

TEST_CASE("Bitwise AND") {
	std::string const text = R"(
fn main() -> int {
	return 29 & 23;
})";
	auto const registers = test::getRegisters(text);
	CHECK(registers[0] == 21);
}

TEST_CASE("Bitwise AND 2") {
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

TEST_CASE("Bitwise OR") {
	std::string const text = R"(
fn main() -> int {
	return 29 | 23;
})";
	auto const registers = test::getRegisters(text);
	CHECK(registers[0] == 31);
}

TEST_CASE("Bitwise OR 2") {
	std::string const text = R"(
fn main() -> int {
	return 0x01 |
           0x02 |
           0x04 |
           0x08 |
		   0x10 |
		   0x20 |
		   0x40 |
		   0x80;
})";
	auto const registers = test::getRegisters(text);
	CHECK(registers[0] == 0xFF);
}

TEST_CASE("Bitwise OR 3") {
	std::string const text = R"(
fn main() -> int {
	return (1 << 0) |
		   (1 << 1) |
		   (1 << 2) |
		   (1 << 3) |
		   (1 << 4) |
		   (1 << 5) |
		   (1 << 6);
})";
	auto const registers = test::getRegisters(text);
	CHECK(registers[0] == 127);
}

TEST_CASE("Bitwise XOR") {
	std::string const text = R"(
fn main() -> int {
	return 29 ^ 23;
})";
	auto const registers = test::getRegisters(text);
	CHECK(registers[0] == 10);
}

TEST_CASE("Bitwise XOR 2") {
	std::string const text = R"(
fn main() -> int {
	return 0xFF00FF00FF00 ^ 0xFFffFFffFFff;
})";
	auto const registers = test::getRegisters(text);
	CHECK(registers[0] == 0x00FF00FF00FF);
}

TEST_CASE("Bitwise NOT") {
	std::string const text = R"(
fn main() -> int {
	return ~23;
})";
	auto const registers = test::getRegisters(text);
	CHECK(registers[0] == ~u64(23));
}

TEST_CASE("Bitwise NOT 2") {
	std::string const text = R"(
fn main() -> int {
	return ~0xFF00FF00;
})";
	auto const registers = test::getRegisters(text);
	CHECK(registers[0] == 0xFFffFFff00FF00FF);
}
