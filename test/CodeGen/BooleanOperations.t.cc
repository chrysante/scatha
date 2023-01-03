#include <Catch/Catch2.hpp>

#include <string>

#include "VM/Program.h"
#include "VM/VirtualMachine.h"
#include "test/CodeGen/BasicCompiler.h"

using namespace scatha;

TEST_CASE("Return boolean literal") {
    std::string const text = R"(
fn main() -> bool {
	return true;
})";
    auto const registers   = test::getRegisters(text);
    CHECK(registers[0] == 1);
}

TEST_CASE("Logical not") {
    std::string const text = R"(
fn main() -> bool {
	let i = 0;
	return !(i == 1);
})";
    auto const registers   = test::getRegisters(text);
    CHECK(registers[0] == 1);
}

TEST_CASE("Logical or") {
    std::string const text = R"(
fn main() -> bool {
	let a = true;
	let b = false;
	return a || b;
})";
    auto const registers   = test::getRegisters(text);
    CHECK(registers[0] == 1);
}
