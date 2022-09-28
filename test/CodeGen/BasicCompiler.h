#ifndef SCATHA_TEST_CODEGEN_BASICCOMPILER_H_
#define SCATHA_TEST_CODEGEN_BASICCOMPILER_H_

#include <string_view>

#include <utl/vector.hpp>

#include "Basic/Basic.h"
#include "VM/Program.h"
#include "VM/VirtualMachine.h"

namespace scatha::test {
	
	vm::Program compile(std::string_view text);

	vm::VirtualMachine compileAndExecute(std::string_view text);
	
	utl::vector<u64> getRegisters(std::string_view text);
	
}

#endif // SCATHA_TEST_CODEGEN_BASICCOMPILER_H_

