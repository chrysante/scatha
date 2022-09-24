#ifndef SCATHA_ASSEMBLY_ASSEMBLYUTIL_H_
#define SCATHA_ASSEMBLY_ASSEMBLYUTIL_H_

#include <optional>

#include "Assembly/Assembly.h"
#include "VM/OpCode.h"

namespace scatha::assembly {

	vm::OpCode mapInstruction(Instruction);
	
	vm::OpCode mapInstruction(Instruction, Element const&, Element const&);
	
}

#endif // SCATHA_ASSEMBLY_ASSEMBLYUTIL_H_

