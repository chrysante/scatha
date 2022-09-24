#ifndef SCATHA_CODEGEN_ASSEMBLER_H_
#define SCATHA_CODEGEN_ASSEMBLER_H_

#include <array>
#include <iosfwd>

#include <utl/hashmap.hpp>
#include <utl/utility.hpp>
#include <utl/vector.hpp>

#include "Assembly/Assembly.h"
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
		vm::Program assemble();
		
	public:
		friend Assembler& operator<<(Assembler&, Instruction);
		friend Assembler& operator<<(Assembler&, Value8);
		friend Assembler& operator<<(Assembler&, Value16);
		friend Assembler& operator<<(Assembler&, Value32);
		friend Assembler& operator<<(Assembler&, Value64);
		friend Assembler& operator<<(Assembler&, RegisterIndex);
		friend Assembler& operator<<(Assembler&, MemoryAddress);
		friend Assembler& operator<<(Assembler&, Label);
		
	private:
		struct LabelPlaceholder{};
		
		void processInstruction(Instruction);
		void processLabel(Label);
		void process_RR_RV(vm::OpCode RR, vm::OpCode RV);
		void process_RR_RV_RM(vm::OpCode RR, vm::OpCode RV, vm::OpCode RM);
		void registerLabel(Label);
		void registerJumpsite();
		
		void postProcess();
		
		Element eat();
		template <typename T>
		T eatAs();
		
		template <typename T>
		T eat();
		
		/** MARK: put
		 * Family of functions for inserting data into the program during assembly
		 */
		void put(vm::OpCode);
		void put(LabelPlaceholder);
		void put(RegisterIndex);
		void put(MemoryAddress);
		void put(Value8);
		void put(Value16);
		void put(Value32);
		void put(Value64);
		
		/** MARK: insert
		 * Family of functions for inserting data into the assembler
		 */
		void insert(u8 value) { data.push_back(value); }
		template <size_t N>
		void insert(std::array<u8, N> value) {
			for (auto byte: value) { insert(byte); }
		}
		void insert(assembly::Marker m) { insert(decompose(m)); }
		
	private:
		vm::Program* program = nullptr;
		size_t index = 0;
		size_t line = 1;
		Instruction currentInstruction;
		// Mapping Label ID -> Code position.
		utl::hashmap<Label, size_t> labels;
		// List of all code position with a jump target.
		struct Jumpsite {
			size_t index;
			size_t line;
			Label label;
		};
		utl::vector<Jumpsite> jumpsites;
		
		utl::vector<u8> data;
	};
	
}

#endif // SCATHA_CODEGEN_ASSEMBLER_H_


