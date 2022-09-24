#ifndef SCATHA_VM_INSTRUCTION_H_
#define SCATHA_VM_INSTRUCTION_H_

#include <iosfwd>

#include <utl/utility.hpp>
#include <utl/vector.hpp>

#include "Basic/Basic.h"

namespace scatha::vm {
	
	
	/*
	 
	 A program looks like this:
	 u8 [instruction], u8... [arguments]
	 ...
	 
	 MEMORY_POINTER           ==   u8 ptrRegIdx, u8 offset, u8 offsetShift
	 eval(MEMORY_POINTER)     ==   reg[ptrRegIdx] + (offset << offsetShift)
	 sizeof(MEMORY_POINTER)   ==   3
	 
	 */
	enum class OpCode: u8 {
		/// MARK: Register allocation
		allocReg,   // (u8 numRegisters)
		
		/// MARK: Memory allocation
		// places a pointer to beginning of memory section in the argument register
		setBrk,     // (u8 sizeRegIdx)
		
		/// MARK: Function call and return
		// regPtr += regOffset
		// regPtr[-2] = regOffset
		// regPtr[-1] = iptr
		// jmp offset
		call,       // (i32 offset, u8 regOffset)
		
		// iptr = regPtr[-1]
		// regPtr -= regPtr[-2]
		ret,        // ()
		
		// terminates the program
		terminate,  // ()
		
		/// MARK: Loads and stores
		movRR,      // (u8 toRegIdx, u8 fromRegIdx)
		movRV,      // (u8 toRegIdx, u64 value)
		movMR,      // (MEMORY_POINTER, u8 ptrRegIdx)
		movRM,      // (u8 ptrRegIdx, MEMORY_POINTER)
		
		/// MARK: Jumps
		// Jump to the specified offset
		jmp,        // (i32 offset)
		// Jump to the specified offset if equal flag is set
		je,         // (i32 offset)
		// Jump to the specified offset if equal flag is not set
		jne,        // (i32 offset)
		// Jump to the specified offset if less flag is set
		jl,         // (i32 offset)
		// Jump to the specified offset if less flag or equal flag is set
		jle,        // (i32 offset)
		// Jump to the specified offset if less flag and equal flag are not set
		jg,         // (i32 offset)
		// Jump to the specified offset if less flag is not set
		jge,        // (i32 offset)
		
		/// MARK: Comparison
		ucmpRR,        //  (u8 regIdxA, u8 regIdxB)
		icmpRR,        //  (u8 regIdxA, u8 regIdxB)
		ucmpRV,        //  (u8 regIdxA, u8 MEMORY_POINTER)
		icmpRV,        //  (u8 regIdxA, u8 MEMORY_POINTER)
		fcmpRR,        //  (u8 regIdxA, u8 regIdxB)
		fcmpRV,        //  (u8 regIdxA, u8 MEMORY_POINTER)
		
		/// MARK: Integer arithmetic
		// reg[regIdxA] += reg[regIdxA]
		addRR,      // (u8 regIdxA, u8 regIdxA)
		// reg[regIdx] += value
		addRV,      // (u8 regIdx, u64 value)
		// reg[regIdxA] += memory[eval(MEMORY_POINTER)]
		addRM,      // (u8 regIdxA, MEMORY_POINTER)
		// reg[regIdxA] -= reg[regIdxA]
		subRR,      // (u8 regIdxA, u8 regIdxA)
		// reg[regIdx] -= value
		subRV,      // (u8 regIdx, u64 value)
		// reg[regIdxA] -= memory[eval(MEMORY_POINTER)]
		subRM,      // (u8 regIdxA, MEMORY_POINTER)
		// reg[regIdxA] *= reg[regIdxA]
		mulRR,      // (u8 regIdxA, u8 regIdxA)
		// reg[regIdx] *= value
		mulRV,      // (u8 regIdx, u64 value)
		// reg[regIdxA] *= memory[eval(MEMORY_POINTER)]
		mulRM,      // (u8 regIdxA, MEMORY_POINTER)
		// reg[regIdxA] /= reg[regIdxA]
		divRR,      // (u8 regIdxA, u8 regIdxA)
		// reg[regIdx] /= value
		divRV,      // (u8 regIdx, u64 value)
		// reg[regIdxA] /= memory[eval(MEMORY_POINTER)]
		divRM,      // (u8 regIdxA, MEMORY_POINTER)
		// reg[regIdxA] %= reg[regIdxA]
		remRR,      // (u8 regIdxA, u8 regIdxA)
		// reg[regIdx] %= value
		remRV,      // (u8 regIdx, u64 value)
		// reg[regIdxA] %= memory[eval(MEMORY_POINTER)]
		remRM,      // (u8 regIdxA, MEMORY_POINTER)
		
		/// MARK: Floating point arithmetic
		// reg[regIdxA] += reg[regIdxA]
		faddRR,      // (u8 regIdxA, u8 regIdxA)
		// reg[regIdx] += value
		faddRV,      // (u8 regIdx, u64 value)
		// reg[regIdxA] += memory[eval(MEMORY_POINTER)]
		faddRM,      // (u8 regIdxA, MEMORY_POINTER)
		// reg[regIdxA] -= reg[regIdxA]
		fsubRR,      // (u8 regIdxA, u8 regIdxA)
		// reg[regIdx] -= value
		fsubRV,      // (u8 regIdx, u64 value)
		// reg[regIdxA] -= memory[eval(MEMORY_POINTER)]
		fsubRM,      // (u8 regIdxA, MEMORY_POINTER)
		// reg[regIdxA] *= reg[regIdxA]
		fmulRR,      // (u8 regIdxA, u8 regIdxA)
		// reg[regIdx] *= value
		fmulRV,      // (u8 regIdx, u64 value)
		// reg[regIdxA] *= memory[eval(MEMORY_POINTER)]
		fmulRM,      // (u8 regIdxA, MEMORY_POINTER)
		// reg[regIdxA] /= reg[regIdxA]
		fdivRR,      // (u8 regIdxA, u8 regIdxA)
		// reg[regIdx]  /= value
		fdivRV,      // (u8 regIdx, u64 value)
		// reg[regIdxA] /= memory[eval(MEMORY_POINTER)]
		fdivRM,      // (u8 regIdxA, MEMORY_POINTER)
		
		/// MARK: Misc
		// extFunctionTable[tableIdx][idxIntoTable](reg[regIdx], this)
		callExt,    // (u8 regIdx, u8 tableIdx, u16 idxIntoTable)
		
		_count
	};
	
	std::ostream& operator<<(std::ostream&, OpCode);
	
	enum class OpCodeClass {
		RR, RV, RM, MR,
		Jump, Other,
		_count
	};
	
	constexpr bool isJump(OpCode c) {
		using enum OpCode;
		return ((u8)c >= (u8)jmp && (u8)c <= (u8)jge) || c == call;
	}
	
	constexpr OpCodeClass classify(OpCode c) {
		using enum OpCodeClass;
		return UTL_MAP_ENUM(c, OpCodeClass, {
			{ OpCode::allocReg,  Other },
			{ OpCode::setBrk,    Other },
			{ OpCode::call,      Other },
			{ OpCode::ret,       Other },
			{ OpCode::terminate, Other },
			{ OpCode::movRR,     RR },
			{ OpCode::movRV,     RV },
			{ OpCode::movMR,     MR },
			{ OpCode::movRM,     RM },
			{ OpCode::jmp,       Jump },
			{ OpCode::je,        Jump },
			{ OpCode::jne,       Jump },
			{ OpCode::jl,        Jump },
			{ OpCode::jle,       Jump },
			{ OpCode::jg,        Jump },
			{ OpCode::jge,       Jump },
			{ OpCode::ucmpRR,    RR },
			{ OpCode::icmpRR,    RR },
			{ OpCode::ucmpRV,    RV },
			{ OpCode::icmpRV,    RV },
			{ OpCode::fcmpRR,    RR },
			{ OpCode::fcmpRV,    RV },
			{ OpCode::addRR,     RR },
			{ OpCode::addRV,     RV },
			{ OpCode::addRM,     RM },
			{ OpCode::subRR,     RR },
			{ OpCode::subRV,     RV },
			{ OpCode::subRM,     RM },
			{ OpCode::mulRR,     RR },
			{ OpCode::mulRV,     RV },
			{ OpCode::mulRM,     RM },
			{ OpCode::divRR,     RR },
			{ OpCode::divRV,     RV },
			{ OpCode::divRM,     RM },
			{ OpCode::remRR,     RR },
			{ OpCode::remRV,     RV },
			{ OpCode::remRM,     RM },
			{ OpCode::faddRR,    RR },
			{ OpCode::faddRV,    RV },
			{ OpCode::faddRM,    RM },
			{ OpCode::fsubRR,    RR },
			{ OpCode::fsubRV,    RV },
			{ OpCode::fsubRM,    RM },
			{ OpCode::fmulRR,    RR },
			{ OpCode::fmulRV,    RV },
			{ OpCode::fmulRM,    RM },
			{ OpCode::fdivRR,    RR },
			{ OpCode::fdivRV,    RV },
			{ OpCode::fdivRM,    RM },
			{ OpCode::callExt,   Other },
		});
	}
	
	constexpr size_t codeSize(OpCode c) {
		using enum OpCodeClass;
		auto const opCodeClass = classify(c);
		if (opCodeClass == Other) {
			switch (c) {
				case OpCode::allocReg:
					return 2;
				case OpCode::setBrk:
					return 2;
				case OpCode::call:
					return 6;
				case OpCode::ret:
					return 1;
				case OpCode::terminate:
					return 1;
				case OpCode::callExt:
					return 5;
				SC_NO_DEFAULT_CASE();
			}
		}
		return UTL_MAP_ENUM(opCodeClass, size_t, {
			{ RR,    3 },
			{ RV,   10 },
			{ RM,    5 },
			{ MR,    5 },
			{ Jump,  5 },
			{ Other, (size_t)-1 }
		});
	}
	
	static_assert((size_t)OpCode::_count < 255,
				  "We reserve this code for something");
	
	using Instruction = u64(*)(u8 const*, u64*, class VirtualMachine*);

	utl::vector<Instruction> makeInstructionTable();
	
}

#endif // SCATHA_VM_INSTRUCTION_H_

