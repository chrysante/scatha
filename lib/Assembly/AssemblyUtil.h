#ifndef SCATHA_ASSEMBLY_ASSEMBLYUTIL_H_
#define SCATHA_ASSEMBLY_ASSEMBLYUTIL_H_

#include <iosfwd>
#include <optional>

#include "Assembly/Assembly.h"
#include "Sema/SymbolTable.h"
#include "VM/OpCode.h"

namespace scatha::assembly {

	vm::OpCode mapInstruction(Instruction);
	vm::OpCode mapInstruction(Instruction, Element const&, Element const&);
	
	class Assembler;
	
	void print(Assembler const&, sema::SymbolTable const&);
	void print(Assembler const&, sema::SymbolTable const&, std::ostream&);
	
}

#endif // SCATHA_ASSEMBLY_ASSEMBLYUTIL_H_

