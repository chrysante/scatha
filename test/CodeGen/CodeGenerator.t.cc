#include <Catch/Catch2.hpp>

#include <string>

#include <utl/bit.hpp>

#include "test/CodeGen/BasicCompiler.h"
#include "VM/Program.h"
#include "VM/VirtualMachine.h"

using namespace scatha;

TEST_CASE("First entire compilation and execution", "[codegen]") {
	std::string const text = R"(
fn main() -> int {
	let a = 1;
	let b = 2;
	return a + b;
}
)";
	
	auto const vm = test::compileAndExecute(text);
	auto const& state = vm.getState();
	CHECK(state.registers[0] == 3);
}

TEST_CASE("Simplest non-trivial program", "[codegen]") {
	std::string const text = R"(
	fn main() -> int {
		return 1;
	}
	)";
	
	auto const vm = test::compileAndExecute(text);
	auto const& state = vm.getState();
	CHECK(state.registers[0] == 1);
}

TEST_CASE("Simple arithmetic", "[codegen]") {
	std::string text;
	u64 expectation = 0;
	SECTION("Addition") {
		text = R"(
	fn main() -> int {
		let a = 1;
		let b = 2;
		return a + b;
	}
	)";
		expectation = 3;
	}
	SECTION("Subtraction") {
		text = R"(
	fn main() -> int {
		let a = 1;
		let b = 2;
		return a - b;
	}
	)";
		expectation = static_cast<u64>(-1);
	}
	SECTION("Multiplication") {
		text = R"(
	fn main() -> int {
		let a = 4;
		let b = -23;
		return a * b;
	}
	)";
		expectation = static_cast<u64>(-92);
	}
	SECTION("Division") {
		text = R"(
	fn main() -> int {
		let a = 100;
		let b = 4;
		return a / b;
	}
	)";
		expectation = 25;
	}
	SECTION("Remainder") {
		text = R"(
	fn main() -> int {
		let a = 100;
		let b = 17;
		return a % b;
	}
	)";
		expectation = 15;
	}
	SECTION("Float Addition") {
		text = R"(
	fn test() -> float {
		let a = 1.3;
		let b = 2.3;
		return a + b;
	}
	)";
		expectation = utl::bit_cast<u64>(1.3 + 2.3);
	}
	SECTION("Float Mutliplication") {
		text = R"(
	fn test() -> float {
		let a = 1.3;
		let b = 2.3;
		return a * b;
	}
	)";
		expectation = utl::bit_cast<u64>(1.3 * 2.3);
	}
	SECTION("Float Subtraction") {
		text = R"(
	fn test() -> float {
		let a = 1.4;
		let b = 2.3;
		return a - b;
	}
	)";
		expectation = utl::bit_cast<u64>(1.4 - 2.3);
	}
	SECTION("Float Division") {
		text = R"(
	fn test() -> float {
		let a = 1.4;
		let b = 2.3;
		return a / b;
	}
	)";
		expectation = utl::bit_cast<u64>(1.4 / 2.3);
	}
	
	SECTION("More complex expressions") {
		text = R"(
	fn main() -> int {
		let a = 12;
		let b = 2;
		let c = 4;
		return (a + b * c) / 2;
	}
	)";
		expectation = 10;
	}
	
	SECTION("Even more complex expressions") {
		text = R"(
	fn main() -> int {
		let a = 12;
		var b = 0;
		let c = 4;
		b += 2;
		return (a + b * c) / 2;
	}
	)";
		expectation = 10;
	}
	
	auto const vm = test::compileAndExecute(text);
	auto const& state = vm.getState();
	CHECK(state.registers[0] == expectation);
}

