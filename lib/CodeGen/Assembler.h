#ifndef SCATHA_CODEGEN_ASSEMBLER_H_
#define SCATHA_CODEGEN_ASSEMBLER_H_

#include <iosfwd>

#include "Basic/Basic.h"
#include "Basic/Memory.h"
#include "CodeGen/AssemblyUtil.h"
#include "VM/OpCode.h"
#include "VM/Program.h"

namespace scatha::codegen {
	
	/*
	 Before being assembled the program looks like this:
	 ---------------------------------------------------
	 [8 bit marker][8 bit opcode][arguments and data]
	 ...
	 ...
	 [8 bit marker][32 bit label]
	 ...
	 ---------------------------------------------------
	 
	 Markers are defined in "AssemblyUtil.h". They are useul
	 for identifying labels and for debugging.
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
		
		friend Assembler& operator<<(Assembler&, vm::OpCode);
		friend Assembler& operator<<(Assembler&, RR);
		friend Assembler& operator<<(Assembler&, RV);
		friend Assembler& operator<<(Assembler&, RM);
		friend Assembler& operator<<(Assembler&, MR);
		friend Assembler& operator<<(Assembler&, Label);
		friend Assembler& operator<<(Assembler&, u8);
		friend Assembler& operator<<(Assembler&, i8);
		friend Assembler& operator<<(Assembler&, u16);
		friend Assembler& operator<<(Assembler&, i16);
		friend Assembler& operator<<(Assembler&, u32);
		friend Assembler& operator<<(Assembler&, i32);
		friend Assembler& operator<<(Assembler&, u64);
		friend Assembler& operator<<(Assembler&, i64);
		friend Assembler& operator<<(Assembler&, f64);
		
		friend void print(Assembler const&);
		friend void print(Assembler const&, std::ostream&);
		
	private:
		utl::vector<u8> instructions;
	};
	
}

#endif // SCATHA_CODEGEN_ASSEMBLER_H_

