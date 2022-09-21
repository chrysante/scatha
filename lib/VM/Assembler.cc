#include "Assembler.h"

#include <array>
#include <iostream>
#include <iomanip>
#include <span>

#include <utl/bit.hpp>
#include <utl/hashmap.hpp>

#include "Basic/Memory.h"

namespace scatha::vm {

	Program Assembler::assemble() {
		utl::hashmap<LabelType, size_t> labelPositions;
		
		Program program;
		
		auto push = [&](size_t iptr, size_t N) {
			for (size_t i = iptr; i < iptr + N; ++i) {
				program.instructions.push_back(instructions[i]);
			}
		};
		
		// Identify all the labels in the copy pass
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
	
	template <typename T>
	static auto printAs(std::span<u8 const> data, size_t offset) {
		return +read<T>(&data[offset]);
	}
	
	static void printInstructions(std::span<u8 const> data, std::ostream& str, bool hasMarkers) {
		auto printMemoryAcccess = [&](size_t i) {
			str << "memory[R[" << printAs<u8>(data, i) << "] + " << printAs<u8>(data, i + 1) << " * " << (1 << printAs<u8>(data, i + 2)) << "]";
		};
		
		for (size_t i = 0; i < data.size(); ) {
			if (hasMarkers) {
				auto const marker = (Marker)data[i];
				SC_ASSERT(marker == Marker::OpCode || marker == Marker::Label, "");
				++i;
				if (marker == Marker::Label) {
					str << ".L" << printAs<LabelType>(data, i) << '\n';
					i += sizeof(LabelType);
					continue;
				}
			}
			SC_ASSERT(i < data.size(), "");
			OpCode const opcode = (OpCode)data[i];
			str << std::setw(3) << i << ": " << opcode << " ";
			
			auto const opcodeClass = classify(opcode);
			switch (opcodeClass) {
				using enum OpCodeClass;
				case RR:
					str << "R[" << printAs<u8>(data, i + 1) << "], R[" << printAs<u8>(data, i + 2) << "]";
					break;
				case RV:
					str << "R[" << printAs<u8>(data, i + 1) << "], " << printAs<u64>(data, i + 2);
					break;
				case RM:
					str << "R[" << printAs<u8>(data, i + 1) << "], "; printMemoryAcccess(i + 2);
					break;
				case MR:
					printMemoryAcccess(i + 1);
					str << ", R[" << printAs<u8>(data, i + 4) << "]";
					break;
				case Jump:
					str << printAs<i32>(data, i + 1);
					break;
				case Other:
					switch (opcode) {
						case OpCode::allocReg:
							str << printAs<u8>(data, i + 1);
							break;
						case OpCode::call:
							str << printAs<i32>(data, i + 1) << ", " << printAs<u8>(data, i + 5);
							break;
						case OpCode::ret:
							break;
						case OpCode::terminate:
							break;
						case OpCode::callExt:
							str << printAs<u8>(data, i + 1) << ", " << printAs<u8>(data, i + 2) << ", " << printAs<u16>(data, i + 3);
							break;
						SC_NO_DEFAULT_CASE();
					}
					break;
				case _count:
					SC_DEBUGFAIL();
			}
			
			
			str << '\n';
			i += ijmp(opcode);
		}
	}
	
	void printProgram(Program const& p) {
		printInstructions(p.instructions, std::cout, false);
	}
	
	void Assembler::print() const {
		printInstructions(instructions, std::cout, true);
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
