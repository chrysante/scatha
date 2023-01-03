#include <Catch/Catch2.hpp>

#include <string>

#include <utl/bit.hpp>

#include "VM/Program.h"
#include "VM/VirtualMachine.h"
#include "test/CodeGen/BasicCompiler.h"

using namespace scatha;

TEST_CASE("First entire compilation and execution", "[codegen]") {
    std::string const text = R"(
fn main() -> int {
	let a = 1;
	let b = 2;
	return a + b;
})";

    auto const vm     = test::compileAndExecute(text);
    auto const& state = vm.getState();
    CHECK(state.registers[0] == 3);
}

TEST_CASE("Simplest non-trivial program", "[codegen]") {
    std::string const text = R"(
fn main() -> int {
	return 1;
})";

    auto const vm     = test::compileAndExecute(text);
    auto const& state = vm.getState();
    CHECK(state.registers[0] == 1);
}

TEST_CASE("Addition", "[codegen]") {
    std::string const text = R"(
fn main() -> int {
	let a = 1;
	let b = 2;
	return a + b;
})";
    auto const registers   = test::getRegisters(text);
    CHECK(registers[0] == 3);
}

TEST_CASE("Subtraction", "[codegen]") {
    std::string const text = R"(
fn main() -> int {
	let a = 1;
	let b = 2;
	return a - b;
})";
    auto const registers   = test::getRegisters(text);
    CHECK(registers[0] == static_cast<u64>(-1));
}

TEST_CASE("Multiplication", "[codegen]") {
    std::string const text = R"(
fn main() -> int {
	let a = 4;
	let b = -23;
	return a * b;
})";
    auto const registers   = test::getRegisters(text);
    CHECK(registers[0] == static_cast<u64>(-92));
}

TEST_CASE("Division", "[codegen]") {
    std::string const text = R"(
fn main() -> int {
	let a = 100;
	let b = 4;
	return a / b;
})";
    auto const registers   = test::getRegisters(text);
    CHECK(registers[0] == 25);
}

TEST_CASE("Remainder", "[codegen]") {
    std::string const text = R"(
fn main() -> int {
	let a = 100;
	let b = 17;
	return a % b;
})";
    auto const registers   = test::getRegisters(text);
    CHECK(registers[0] == 15);
}

//TEST_CASE("Float Addition", "[codegen]") {
//    std::string const text = R"(
//fn test() -> float {
//	let a = 1.3;
//	let b = 2.3;
//	return a + b;
//})";
//    auto const registers   = test::getRegisters(text);
//    CHECK(registers[0] == utl::bit_cast<u64>(1.3 + 2.3));
//}
//
//TEST_CASE("Float Mutliplication", "[codegen]") {
//    std::string const text = R"(
//fn test() -> float {
//	let a = 1.3;
//	let b = 2.3;
//	return a * b;
//})";
//    auto const registers = test::getRegisters(text);
//    CHECK(registers[0] == utl::bit_cast<u64>(1.3 * 2.3));
//}
//
//TEST_CASE("Float Subtraction", "[codegen]") {
//    std::string const text = R"(
//fn test() -> float {
//	let a = 1.4;
//	let b = 2.3;
//	return a - b;
//})";
//    auto const registers   = test::getRegisters(text);
//    CHECK(registers[0] == utl::bit_cast<u64>(1.4 - 2.3));
//}
//
//TEST_CASE("Float Division", "[codegen]") {
//    std::string const text = R"(
//fn test() -> float {
//	let a = 1.4;
//	let b = 2.3;
//	return a / b;
//})";
//    auto const registers   = test::getRegisters(text);
//    CHECK(registers[0] == utl::bit_cast<u64>(1.4 / 2.3));
//}

TEST_CASE("More complex expressions", "[codegen]") {
    std::string const text = R"(
fn main() -> int {
	let a = 12;
	let b = 2;
	let c = 4;
	return (a + b * c) / 2;
})";
    auto const registers   = test::getRegisters(text);
    CHECK(registers[0] == 10);
}

TEST_CASE("Even more complex expressions", "[codegen]") {
    std::string const text = R"(
fn main() -> int {
	let a = 12;
	var b = 0;
	let c = 4;
	b += 2;
	return 0, (a + b * c) / 2;
})";
    auto const registers   = test::getRegisters(text);
    CHECK(registers[0] == 10);
}
