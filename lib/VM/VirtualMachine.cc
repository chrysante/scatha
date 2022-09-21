#include "VM/VirtualMachine.h"

#include <iostream>

#include "Basic/Memory.h"
#include "VM/OpCode.h"

namespace scatha::vm {
	
	VirtualMachine::VirtualMachine(): instructionTable(makeInstructionTable()) {
		
	}
	
	void VirtualMachine::load(Program const& program) {
		this->program = program.instructions;
	}
	
	void VirtualMachine::execute() {
		size_t icounter = 0;
		while (iptr < program.size()) {
			u8 const opCode = program[iptr];
			SC_ASSERT(opCode < (u8)OpCode::_count, "Invalid op-code");
			++icounter;
			auto const instruction = instructionTable[opCode];
			iptr += instruction(program.data() + iptr + 1, regPtr, this);
		}
		SC_ASSERT(iptr == program.size(), "");
		
		std::cout << "Executed " << icounter << " Instructions\n";
	}
	
}
