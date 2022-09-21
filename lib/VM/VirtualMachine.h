#ifndef SCATHA_VM_VIRTUALMACHINE_H_
#define SCATHA_VM_VIRTUALMACHINE_H_

#include <utl/vector.hpp>
#include <utl/stack.hpp>

#include "Basic/Basic.h"
#include "VM/OpCode.h"
#include "VM/Program.h"

namespace scatha::vm {
	
	class VirtualMachine {
	public:
		VirtualMachine();
		void load(Program const&);
		void execute();
		
//	private:
		struct Flags {
			bool less     : 1;
			bool equal    : 1;
		};
		
		using ExternalFunction = void(*)(u64, VirtualMachine*);
		
//	private:
		u64 iptr = 0;
		u64* regPtr = nullptr;
		Flags flags{};
		
		utl::vector<Instruction> instructionTable;
		utl::vector<utl::vector<ExternalFunction>> extFunctionTable;
		utl::vector<u8> program;
		
		utl::vector<u64> registers;
		utl::vector<u8> memory;
	};
	
}

#endif // SCATHA_VM_VIRTUALMACHINE_H_

