#ifndef SCATHA_VM_VIRTUALMACHINE_H_
#define SCATHA_VM_VIRTUALMACHINE_H_

#include <utl/vector.hpp>
#include <utl/stack.hpp>

#include "Basic/Basic.h"
#include "VM/OpCode.h"
#include "VM/Program.h"

namespace scatha::vm {
	
	struct VMFlags {
		bool less     : 1;
		bool equal    : 1;
	};
	
	using ExternalFunction = void(*)(u64, class VirtualMachine*);
	
	struct VMState {
		VMState() = default;
		VMState(VMState const&) = delete;
		VMState(VMState&&) = default;
		
		u8 const* iptr = nullptr;
		u64* regPtr = nullptr;
		VMFlags flags{};
		
		utl::vector<u64> registers;
		utl::vector<u8> memory;
		
		u8 const* programBreak = nullptr;
		u8* memoryPtr = nullptr;
		u8 const* memoryBreak = nullptr;
		
		size_t instructionCount = 0;
	};
	
	struct VMStats {
		size_t executedInstructions;
	};
	
	class SCATHA(API) VirtualMachine: VMState {
	public:
		VirtualMachine();
		void load(Program const&);
		void execute();
		
		void addExternalFunction(size_t slot, ExternalFunction);
		
		VMState const& getState() const { return static_cast<VMState const&>(*this); }
		VMStats const& getStats() const { return stats; }
		
		friend struct OpCodeImpl;
		
	private:
		void resizeMemory(size_t newSize);
		void cleanup();
		
	private:
		utl::vector<Instruction> instructionTable;
		utl::vector<utl::vector<ExternalFunction>> extFunctionTable;
		VMStats stats;
	};
	
}

#endif // SCATHA_VM_VIRTUALMACHINE_H_

