#include <Catch/Catch2.hpp>

#include <string>

#include "test/CodeGen/BasicCompiler.h"
#include "VM/Program.h"
#include "VM/VirtualMachine.h"

using namespace scatha;

TEST_CASE("Recursive euclidean algorithm", "[codegen]") {
	std::string const text = R"(
	
 fn gcd(a: int, b: int) -> int {
	 if (b == 0) {
		 return a;
	 }
	 return gcd(b, a % b);
 }

 fn main() -> int {
	 let a = 756476;
	 let b = 1253;
  
	 return gcd(a, b);
 }

	)";
	
	auto const vm = test::compileAndExecute(text);
	auto const& state = vm.getState();
	CHECK(state.registers[0] == 7);
}

TEST_CASE("Recursive fibonacci", "[codegen]") {
	std::string const text = R"(
	
 fn fib(n: int) -> int {
	if (n == 0) {
		return 0;
	}
	if (n == 1) {
		return 1;
	}
	return fib(n - 1) + fib(n - 2);
 }

 fn main() -> int {
	let n = 10;
	return fib(n);
 }

	)";
	
	auto const vm = test::compileAndExecute(text);
	auto const& state = vm.getState();
	CHECK(state.registers[0] == 55);
}

TEST_CASE("Recursive factorial", "[codegen]") {
	std::string const text = R"(
	
 fn fact(n: int) -> int {
	if (n <= 1) {
		return 1;
	}
	return n * fact(n - 1);
 }

 fn main() -> int {
	return fact(10);
 }

	)";
	
	auto const vm = test::compileAndExecute(text);
	auto const& state = vm.getState();
	CHECK(state.registers[0] == 3628800);
}
