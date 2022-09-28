#include "BasicCompiler.h"

#include "Assembly/Assembler.h"
#include "Basic/Memory.h"
#include "CodeGen/CodeGenerator.h"
#include "IC/TacGenerator.h"
#include "IC/Canonicalize.h"
#include "Lexer/Lexer.h"
#include "Parser/Parser.h"
#include "Sema/Analyze.h"
#include "VM/Program.h"
#include "VM/VirtualMachine.h"

namespace scatha::test {
	
	vm::Program compile(std::string_view text) {
		lex::Lexer l(text);
		auto tokens = l.lex();
		parse::Parser p(tokens);
		auto ast = p.parse();
		auto sym = sema::analyze(ast.get());
		ic::canonicalize(ast.get());
		ic::TacGenerator t(sym);
		auto const tac = t.run(ast.get());
		codegen::CodeGenerator cg(tac);
		auto const str = cg.run();
		assembly::Assembler a(str);
		
		// start execution with main if it exists
		auto const mainID = sym.lookupName(Token("main"));
		return a.assemble({
			.mainID = mainID.id()
		});
	}

	vm::VirtualMachine compileAndExecute(std::string_view text) {
		vm::Program const p = compile(text);
		vm::VirtualMachine vm;
		vm.load(p);
		vm.execute();
		return vm;
	}
	
	utl::vector<u64> getRegisters(std::string_view text) {
		auto vm = test::compileAndExecute(text);
		return vm.getState().registers;
	}
	
}
