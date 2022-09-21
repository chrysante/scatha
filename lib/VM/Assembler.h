#ifndef SCATHA_VM_ASSEMBLER_H_
#define SCATHA_VM_ASSEMBLER_H_

#include "Basic/Basic.h"
#include "Basic/Memory.h"
#include "VM/OpCode.h"
#include "VM/Program.h"
#include "VM/Label.h"

namespace scatha::vm {
	
	namespace internal {
		template <typename T>
		auto convertValue(T x) {
			if constexpr (std::floating_point<T>) {
				return utl::bit_cast<u64>(static_cast<f64>(x));
			}
			else {
				return static_cast<u64>(x);
			}
		}
		template <typename T>
		concept Arithmetic = std::integral<T> || std::floating_point<T>;
	}
	
	struct RR {
		explicit RR(u8 a, u8 b): a(a), b(b) {}
		u8 a, b;
	};
	struct RV {
		explicit RV(u8 r, internal::Arithmetic auto v): r(r), v(internal::convertValue(v)) {}
		u8 r;
		u64 v;
	};
	struct RM {
		explicit RM(u8 r, u8 ptrRegIdx, u8 offset, u8 offsetShift):
			r(r),
			ptrRegIdx(ptrRegIdx),
			offset(offset),
			offsetShift(offsetShift)
		{}
		u8 r;
		u8 ptrRegIdx;
		u8 offset;
		u8 offsetShift;
	};
	struct MR {
		explicit MR(u8 ptrRegIdx, u8 offset, u8 offsetShift, u8 r):
			ptrRegIdx(ptrRegIdx),
			offset(offset),
			offsetShift(offsetShift),
			r(r)
		{}
		u8 ptrRegIdx;
		u8 offset;
		u8 offsetShift;
		u8 r;
	};
	
	class Assembler {
	public:
		Program assemble();
	
		void print() const;
		
		friend Assembler& operator<<(Assembler&, OpCode);
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
		
	private:
		template <typename T>
		auto printAs(size_t index) const { return +read<T>(&instructions[index]); }
		
	private:
		utl::vector<u8> instructions;
	};
	
	void printProgram(Program const&);

	enum class Marker: u8 {
		Label = 0x80,
		OpCode = 0xFF,
	};
	
}

#endif // SCATHA_VM_ASSEMBLER_H_

