#include <Catch/Catch2.hpp>

#include <string>

#include "test/CodeGen/BasicCompiler.h"
#include "VM/Program.h"
#include "VM/VirtualMachine.h"

using namespace scatha;


TEST_CASE("Local scopes") {
	
	std::string const text = R"(
fn main() -> int {
	let x = 0;
	{
		// local name x shadows outer x
		let x = 1;
		return x;
	}
}
)";
	
	auto const vm = test::compileAndExecute(text);
	auto const& state = vm.getState();
	
	CHECK(state.registers[0] == 1);
}

TEST_CASE() {
	
	std::string const text = R"(
fn main() -> int {
	let x = 0;
	{
		// local name x shadows outer x
		let x = 1;
	}
	{
		// and again
		let x = 2;
		return x;
	}
}
)";
	
	auto const vm = test::compileAndExecute(text);
	auto const& state = vm.getState();
	
	CHECK(state.registers[0] == 2);
}
