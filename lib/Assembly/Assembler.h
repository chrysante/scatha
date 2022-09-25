#ifndef SCATHA_CODEGEN_ASSEMBLER_H_
#define SCATHA_CODEGEN_ASSEMBLER_H_

#include <array>
#include <iosfwd>

#include <utl/hashmap.hpp>
#include <utl/utility.hpp>
#include <utl/vector.hpp>

#include "Assembly/Assembly.h"
#include "Assembly/AssemblyStream.h"
#include "Basic/Basic.h"
#include "VM/Program.h"
#include "VM/OpCode.h"

namespace scatha::assembly {
	
	/*
	 Before being assembled the program looks like this:
	 ---------------------------------------------------
	 [8 bit marker] -- some data --
	 ...
	 ...
	 ...
	 ---------------------------------------------------
	 
	 Markers are defined in "Assembly.h".
	 They are stripped from the program in assembly stage.
	 After assembly the executable looks like this:
	 ---------------------------------------------------
	 [8 bit opcode][arguments and data]
	 ...
	 ---------------------------------------------------
	 */
	
	class Assembler {
	public:
		explicit Assembler(AssemblyStream const&);
		vm::Program assemble();
		
	private:
		struct LabelPlaceholder{};
		
		void processInstruction(Instruction);
		void processBinaryInstruction(Instruction);
		void processJump(Instruction);
		void processLabel(Label);
		
		void registerLabel(Label);
		void registerJumpsite();
		
		void postProcess();
		
		/** MARK: eat
		 * Family of functions for extracting data out of the assembly stream
		 */
		Element eat();
		template <typename T>
		T eatAs();
		template <typename T>
		T eatImpl();
		
		/** MARK: put
		 * Family of functions for inserting data into the program during assembly
		 */
		void put(vm::OpCode);
		void put(LabelPlaceholder);
		void put(Element const&);
		void put(RegisterIndex);
		void put(MemoryAddress);
		void put(Value8);
		void put(Value16);
		void put(Value32);
		void put(Value64);
		
	private:
		struct Jumpsite {
			size_t index;
			size_t line;
			Label label;
		};
		
		// The AssemblyStream we are processing
		AssemblyStream const& stream;
		// Pointer to the program we are assembling, so we don't have to pass
		// it around the call tree
		vm::Program* program = nullptr;
		// Index into the AssemblyStream
		size_t index = 0;
		// Current line of the AssemblyStream
		size_t line = 1;
		// Instruction currently being processed
		Instruction currentInstruction;
		// Mapping Label ID -> Code position
		utl::hashmap<Label, size_t> labels;
		// List of all code position with a jump site
		utl::vector<Jumpsite> jumpsites;
	};
	
}

#endif // SCATHA_CODEGEN_ASSEMBLER_H_


