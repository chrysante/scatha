#include <Catch/Catch2.hpp>

#include <string>

#include "test/CodeGen/BasicCompiler.h"
#include "VM/Program.h"
#include "VM/VirtualMachine.h"

using namespace scatha;

TEST_CASE("Conditions") {
	
	std::string text;
	
	SECTION("fcmp greater var-lit") {
		text = R"(
fn main() -> int {
	let a = 32.1;
	if a > 12.2 {
		return 1;
	}
	else {
		return 2;
	}
})";
	}
	SECTION("fcmp greater lit-var") {
		text = R"(
fn main() -> int {
	let a = 32.1;
	if 100.0 > a {
		return 1;
	}
	else {
		return 2;
	}
})";
	}
	SECTION("fcmp less var-lit") {
		text = R"(
fn main() -> int {
	let a = 32.1;
	if a < 112.2 {
		return 1;
	}
	else {
		return 2;
	}
})";
	}
	SECTION("fcmp less lit-var") {
		text = R"(
fn main() -> int {
	let a = 32.1;
	if -1002.0 < a {
		return 1;
	}
	else {
		return 2;
	}
})";
	}
	SECTION("fcmp less lit-lit") {
		text = R"(
fn main() -> int {
	let a = 32.1;
	if -1002.0 < 0.0 {
		return 1;
	}
	else {
		return 2;
	}
})";
	}
	SECTION("nested if-else-if") {
		text = R"(
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
	}
	SECTION("") {
		text = R"(
fn main() -> int {
	let x = 0;
	if -1002.0 > 0.0 {
		x = 0;
	}
	else {
		x = 1;
	}
	let y = 1 + 2 * 3 / 4 % 5 / 6;
	if x == 1 {
		return x;
	}
	else {
		return x + 100;
	}
})";
	}
	
	auto vm = test::compileAndExecute(text);
	auto const& state = vm.getState();
	CHECK(state.registers[0] == 1);
}
