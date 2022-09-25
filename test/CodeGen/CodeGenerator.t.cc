#define SC_TEST

#include <Catch/Catch2.hpp>

#include <string>

#include "Assembly/Assembler.h"
#include "Basic/Memory.h"
#include "CodeGen/CodeGenerator.h"
#include "IC/TacGenerator.h"
#include "IC/Canonicalize.h"
#include "Lexer/Lexer.h"
#include "Parser/Parser.h"
#include "Sema/SemanticAnalyzer.h"
#include "VM/Program.h"
#include "VM/VirtualMachine.h"

using namespace scatha;

static vm::Program compile(std::string_view text) {
	lex::Lexer l(text);
	auto tokens = l.lex();
	parse::Parser p(tokens);
	auto ast = p.parse();
	sema::SemanticAnalyzer s;
	s.run(ast.get());
	ic::canonicalize(ast.get());
	ic::TacGenerator t(s.symbolTable());
	auto const tac = t.run(ast.get());
	codegen::CodeGenerator cg(tac);
	auto const str = cg.run();
	assembly::Assembler a(str);
	return a.assemble();
}

static vm::VirtualMachine compileAndExecute(std::string_view text) {
	vm::Program const p = compile(text);
	vm::VirtualMachine vm;
	vm.load(p);
	vm.execute();
	return vm;
}

TEST_CASE("First entire compilation and execution", "[codegen]") {
	std::string const text = R"(
fn main() -> int {
	let a = 1;
	let b = 2;
	return a + b;
}
)";
	
	auto const vm = compileAndExecute(text);
	auto const& state = vm.getState();
	CHECK(state.registers[0] == 3);
}

TEST_CASE("Simplest possible non-trivial program", "[codegen]") {
	std::string const text = R"(
fn main() -> int {
	return 1;
}
)";
	
	auto const vm = compileAndExecute(text);
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
	
	auto const vm = compileAndExecute(text);
	auto const& state = vm.getState();
	CHECK(state.registers[0] == expectation);
}


