#include <Catch/Catch2.hpp>

#include <string>

#include "test/CodeGen/BasicCompiler.h"
#include "VM/Program.h"
#include "VM/VirtualMachine.h"

using namespace scatha;


TEST_CASE("Return boolean literal") {
	std::string const text = R"(
fn main() -> bool {
	return true;
})";
	auto const registers = test::getRegisters(text);
	CHECK(registers[0] == 1);
}

TEST_CASE("Logical or") {
	std::string const text = R"(
/// This fails to compile because we don't have an implementation for operator|| and operator&& yet,
/// and they are not trivial to implement because of short circuit evaluation.
//fn main() -> bool {
//	let a = true;
//	let b = false;
//	return a || b;
//}
)";
	auto const registers = test::getRegisters(text);
//	CHECK(registers[0] == 1);
}
