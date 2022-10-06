#include <Catch/Catch2.hpp>

#include <string>

#include "VM/Program.h"
#include "VM/VirtualMachine.h"
#include "test/CodeGen/BasicCompiler.h"

using namespace scatha;

TEST_CASE("Recursive euclidean algorithm", "[codegen]") {
    std::string const text  = R"(
fn main() -> int {
	let a = 756476;
	let b = 1253;
	return gcd(a, b);
}
fn gcd(a: int, b: int) -> int {
	if (b == 0) {
		return a;
	}
	return gcd(b, a % b);
})";
    auto const        vm    = test::compileAndExecute(text);
    auto const       &state = vm.getState();
    CHECK(state.registers[0] == 7);
}

TEST_CASE("Recursive fibonacci", "[codegen]") {
    std::string const text  = R"(
fn main() -> int {
	let n = 10;
	return fib(n);
}
fn fib(n: int) -> int {
	if (n == 0) {
		return 0;
	}
	if (n == 1) {
		return 1;
	}
	return fib(n - 1) + fib(n - 2);
})";
    auto const        vm    = test::compileAndExecute(text);
    auto const       &state = vm.getState();
    CHECK(state.registers[0] == 55);
}

TEST_CASE("Recursive factorial", "[codegen]") {
    std::string const text  = R"(
fn main() -> int {
	return fact(10);
}
fn fact(n: int) -> int {
	if n <= 1 {
		return 1;
	}
	return n * fact(n - 1);
})";
    auto const        vm    = test::compileAndExecute(text);
    auto const       &state = vm.getState();
    CHECK(state.registers[0] == 3628800);
}

TEST_CASE("Recursive pow", "[codegen]") {
    std::string const text  = R"(
fn main() -> int {
	 return pow(3, 5);
}
fn pow(base: int, exponent: int) -> int {
	if exponent == 0 {
		return 1;
	}
	if exponent == 1 {
		return base;
	}
	if exponent % 2 == 0 {
		return pow(base *  base, exponent / 2);
	}
	return base * pow(base *  base, exponent / 2);
})";
    auto const        vm    = test::compileAndExecute(text);
    auto const       &state = vm.getState();
    CHECK(state.registers[0] == 243);
}
