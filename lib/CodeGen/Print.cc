#include "Print.h"

#include <ostream>
#include <iomanip>

#include "Basic/Memory.h"
#include "CodeGen/AssemblyUtil.h"
#include "VM/OpCode.h"

namespace scatha::codegen {
	
	using namespace vm;
	
	template <typename T>
	static auto printAs(std::span<u8 const> data, size_t offset) {
		return +read<T>(&data[offset]);
	}
	
	void printInstructions(std::span<u8 const> data, std::ostream& str, PrintDescription desc) {
		auto printMemoryAcccess = [&](size_t i) {
			str << "memory[R[" << printAs<u8>(data, i) << "] + " << printAs<u8>(data, i + 1) << " * " << (1 << printAs<u8>(data, i + 2)) << "]";
		};
		
		for (size_t i = 0; i < data.size(); ) {
			if (desc.codeHasMarkers) {
				auto const marker = (Marker)data[i];
				SC_ASSERT(marker == Marker::OpCode || marker == Marker::Label, "");
				++i;
				if (marker == Marker::Label) {
					str << "LABEL: " << printAs<LabelType>(data, i) << '\n';
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
						case OpCode::setBrk:
							str << printAs<u64>(data, i + 1);
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
	
}
