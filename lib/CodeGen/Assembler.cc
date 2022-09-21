#include "CodeGen/Assembler.h"

#include <array>
#include <iostream>
#include <span>

#include <utl/bit.hpp>
#include <utl/hashmap.hpp>

#include "Basic/Memory.h"
#include "CodeGen/Print.h"

namespace scatha::codegen {
	
	using namespace vm;

	vm::Program Assembler::assemble() {
		Program program;
		
		auto push = [&](size_t iptr, size_t N) {
			for (size_t i = iptr; i < iptr + N; ++i) {
				program.instructions.push_back(instructions[i]);
			}
		};
		
		// Identify all the labels in the copy pass
		utl::hashmap<LabelType, size_t> labelPositions;
		for (size_t iptr = 0; iptr < instructions.size(); ) {
			auto const marker = (Marker)instructions[iptr];
			if (marker != Marker::OpCode && marker != Marker::Label) {
				throw; // Invalid assembly
			}
			++iptr;
			
			switch (marker) {
				case Marker::OpCode: {
					OpCode const opcode = (OpCode)instructions[iptr];
					auto const codeSize = ijmp(opcode);
					push(iptr, codeSize);
					iptr += codeSize;
					break;
				}
				case Marker::Label: {
					auto const [_, success] = labelPositions.insert({
						read<LabelType>(&instructions[iptr]),
						program.instructions.size()
					});
					if (!success) {
						throw; // label declared before
					}
					iptr += sizeof(LabelType);
					break;
				}
			}
		}
		
		// Scan the copied instructions and replace labels by offsets
		for (i64 iptr = 0; iptr < (i64)program.instructions.size(); ) {
			OpCode const opcode = (OpCode)program.instructions[iptr];
			auto const codeSize = ijmp(opcode);
			
			if (isJump(opcode)) {
				LabelType const targetID = read<LabelType>(&program.instructions[iptr + 1]);
				auto const itr = labelPositions.find(targetID);
				if (itr == labelPositions.end()) {
					throw; // use of undeclared label
				}
				i64 const jumpTarget = itr->second;
				store<i32>(&program.instructions[iptr + 1], jumpTarget - iptr);
			}
			
			iptr += codeSize;
		}
		
		return program;
	}
	
	void print(Assembler const& a) {
		print(a, std::cout);
	}
	
	void print(Assembler const& a, std::ostream& str) {
		printInstructions(a.instructions, str, { .codeHasMarkers = true });
	}
	
	Assembler& operator<<(Assembler& p, OpCode x) {
		return p << (u8)0xFF << utl::bit_cast<u8>(x);
	}
	
	Assembler& operator<<(Assembler& p, RR x) {
		return p << x.a << x.b;
	}
	
	Assembler& operator<<(Assembler& p, RV x) {
		return p << x.r << x.v;
	}
	
	Assembler& operator<<(Assembler& p, RM x) {
		return p << x.r << x.ptrRegIdx << x.offset << x.offsetShift;
	}
	
	Assembler& operator<<(Assembler& p, MR x) {
		return p << x.ptrRegIdx << x.offset << x.offsetShift << x.r;
	}
	
	Assembler& operator<<(Assembler& p, Label l) {
		return p << (u8)0x80 << l.value;
	}
	
	Assembler& operator<<(Assembler& p, u8 x) {
		p.instructions.push_back(x);
		return p;
	}
	
	Assembler& operator<<(Assembler& p, i8 x) {
		return p << utl::bit_cast<u8>(x);
	}
	
	Assembler& operator<<(Assembler& p, u16 x) {
		auto const y = utl::bit_cast<std::array<u8, 2>>(x);
		return p << y[0] << y[1];
	}
	
	Assembler& operator<<(Assembler& p, i16 x) {
		return p << utl::bit_cast<u16>(x);
	}
	
	Assembler& operator<<(Assembler& p, u32 x) {
		auto const y = utl::bit_cast<std::array<u16, 2>>(x);
		return p << y[0] << y[1];
	}
	
	Assembler& operator<<(Assembler& p, i32 x) {
		return p << utl::bit_cast<u32>(x);
	}
	
	Assembler& operator<<(Assembler& p, u64 x) {
		auto const y = utl::bit_cast<std::array<u32, 2>>(x);
		return p << y[0] << y[1];
	}
	
	Assembler& operator<<(Assembler& p, i64 x) {
		return p << utl::bit_cast<u64>(x);
	}
	
	Assembler& operator<<(Assembler& p, f64 x) {
		return p << utl::bit_cast<u64>(x);
	}
	
}
