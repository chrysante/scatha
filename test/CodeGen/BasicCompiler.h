#ifndef SCATHA_TEST_CODEGEN_BASICCOMPILER_H_
#define SCATHA_TEST_CODEGEN_BASICCOMPILER_H_

#include <string_view>

#include "VM/Program.h"
#include "VM/VirtualMachine.h"

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

namespace scatha::test {
	
	vm::Program compile(std::string_view text);

	vm::VirtualMachine compileAndExecute(std::string_view text);
	
	utl::vector<u64> getRegisters(std::string_view text);
	
}

#endif // SCATHA_TEST_CODEGEN_BASICCOMPILER_H_

