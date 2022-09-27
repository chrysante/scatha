#include <Catch/Catch2.hpp>

#include <string>

#include "test/CodeGen/BasicCompiler.h"
#include "VM/Program.h"
#include "VM/VirtualMachine.h"

using namespace scatha;

TEST_CASE("while loops") {
	
	std::string const text = R"(
fn fact(n: int) -> int {
  var i = 0;
  var result = 1;
  while i < n {
	  i += 1;
	  result *= i;
  }
  return result;
}

fn main() -> int {
	return fact(4);
}
)";
	
	auto const vm = test::compileAndExecute(text);
	auto const& state = vm.getState();
	CHECK(state.registers[0] == 24);
}

TEST_CASE("iterative gcd") {
	
	std::string const text = R"(

fn gcd(a: int, b: int) -> int {
	while a != b {
		if a > b {
			a -= b;
		}
		else {
			b -= a;
		}
	}
	return a;
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
