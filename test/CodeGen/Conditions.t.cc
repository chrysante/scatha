#include <Catch/Catch2.hpp>

#include <string>

#include "test/CodeGen/BasicCompiler.h"
#include "VM/Program.h"
#include "VM/VirtualMachine.h"

using namespace scatha;

TEST_CASE("fcmp greater var-lit") {
	std::string const text = R"(
fn main() -> int {
	let a = 32.1;
	if a > 12.2 {
		return 1;
	}
	else {
		return 2;
	}
})";
	auto const registers = test::getRegisters(text);
	CHECK(registers[0] == 1);
}
TEST_CASE("fcmp greater lit-var") {
	std::string const text = R"(
fn main() -> int {
	let a = 32.1;
	if 100.0 > a {
		return 1;
	}
	else {
		return 2;
	}
})";
	auto const registers = test::getRegisters(text);
	CHECK(registers[0] == 1);
}
TEST_CASE("fcmp less var-lit") {
	std::string const text = R"(
fn main() -> int {
	let a = 32.1;
	if a < 112.2 {
		return 1;
	}
	else {
		return 2;
	}
})";
	auto const registers = test::getRegisters(text);
	CHECK(registers[0] == 1);
}
TEST_CASE("fcmp less lit-var") {
	std::string const text = R"(
fn main() -> int {
	let a = 32.1;
	if -1002.0 < a {
		return 1;
	}
	else {
		return 2;
	}
})";
	auto const registers = test::getRegisters(text);
	CHECK(registers[0] == 1);
}
TEST_CASE("fcmp less lit-lit") {
	std::string const text = R"(
fn main() -> int {
	let a = 32.1;
	if -1002.0 < 0.0 {
		return 1;
	}
	else {
		return 2;
	}
})";
	auto const registers = test::getRegisters(text);
	CHECK(registers[0] == 1);
}
TEST_CASE("nested if-else-if") {
	std::string const text = R"(
fn main() -> int {
	let x = 0;
	if -1002.0 > 0.0 {
		return 0;
	}
	else if 1002.0 < 0.0 {
		return 0;
	}
	else if -1 < x {
		return 1;
	}
	else {
		return 2;
	}
})";
	auto const registers = test::getRegisters(text);
	CHECK(registers[0] == 1);
}
TEST_CASE("more nested if else") {
	std::string const text = R"(
fn main() -> int {
	let x = 0;
	if -1002.0 > 0.0 {
		x = 0;
	}
	else {
		x = 1;
	}
	// just to throw some more complexity at the compiler
	let y = 1 + 2 * 3 / 4 % 5 / 6;
	if x == 1 {
		return x;
	}
	else {
		return x + 100;
	}
})";
	auto const registers = test::getRegisters(text);
	CHECK(registers[0] == 1);
}

TEST_CASE("") {
	std::string const text = R"(
fn main() -> bool {
	return !false;
})";
	auto const registers = test::getRegisters(text);
	CHECK(registers[0] == 1);
}

TEST_CASE("Branch based on literals") {
	std::string const text = R"(
fn main() -> int {
	if true {
		return 1;
	}
	else {
		return 0;
	}
})";
	auto const registers = test::getRegisters(text);
	CHECK(registers[0] == 1);
}
	
TEST_CASE("Branch based on result of function calls") {
	std::string const text = R"(
fn greaterZero(a: int) -> bool {
	return a > 0;
	return !(a <= 0);
}

fn main() -> int {
	let x = 0;
	let y = 1;
	if greaterZero(x) {
		return 2;
	}
	else if greaterZero(y) {
		return 1;
	}
	else {
		return 0;
	}
})";
	auto const registers = test::getRegisters(text);
	CHECK(registers[0] == 1);
}

