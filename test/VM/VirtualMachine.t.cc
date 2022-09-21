#define SC_TEST

#include <Catch/Catch2.hpp>

#include "CodeGen/Assembler.h"
#include "CodeGen/AssemblyUtil.h"
#include "VM/VirtualMachine.h"

using namespace scatha;
using namespace scatha::codegen;
using namespace scatha::vm;

TEST_CASE("Memory read write", "[vm][codegen]") {
	using enum OpCode;
	
	Assembler a;
	a << allocReg << u8(4);                     // ; allocate 4 registers
	a << movRV    << RV(0, u64(128));           // ; a = 128
	a << setBrk   << u8(0);                     // ; allocate a bytes of memory
												// ; now a = <pointer-to-memory-section>
	a << movRV    << RV(1, i64(-1));            // ; b = -1
	a << movMR    << MR(0,  0, 0, 1);           // ; memory[a] = b
	a << movRV    << RV(2, f64(1.5));           // ; c = 1.5
	a << movMR    << MR(0,  8, 0, 2);           // ; memory[a + 8] = c
	a << movRV    << RV(2, u64(13));            // ; d = 13
	a << movMR    << MR(0, 16, 0, 2);           // ; memory[a + 16] = d
	
	a << terminate;
	
	Program p = a.assemble();
	VirtualMachine vm;
	vm.load(p);
	vm.execute();
	auto const& state = vm.getState();
	
	CHECK(read<i64>(state.memoryPtr) == -1);
	CHECK(read<f64>(state.memoryPtr + 8) == 1.5);
	CHECK(read<u64>(state.memoryPtr + 16) == 13);
}

TEST_CASE("Euclidean algorithm", "[vm][codegen]") {
	
	using enum OpCode;
	
	Assembler a;
	/*
	 should hold the result in the 4th register
	 */
												// ; Main function
	a << allocReg << u8(4);                     // ; allocate 4 registers
	a << movRV    << RV(2, i64(54));            // ; a = 54
	a << movRV    << RV(3, i64(24));            // ; b = 24
	a << call     << makeLabel("gcd") << u8(2);
	
	a << terminate;
	
	a << Label("gcd");                          // ; gcd(i64 a, i64 b):
	a << allocReg << u8(3);
	a << icmpRV   << RV(1, i64(0));             // ; b == 0
	a << jne      << makeLabel("gcd-else");
	a << movRR    << RR(2, 0);                  // ; retval = a
	a << ret;
	a << Label("gcd-else");
	// swap a and b
	a << movRR    << RR(2, 1);                  // ; c = b
	a << movRR    << RR(1, 0);                  // ; b = a
	a << movRR    << RR(0, 2);                  // ; a = c
	a << remRR    << RR(1, 0);
	
	a << jmp      << makeLabel("gcd");          // ; tail call
	
	Program p = a.assemble();
	VirtualMachine vm;
	vm.load(p);
	vm.execute();
	auto const& state = vm.getState();
	
	// gcd(54, 24) == 6
	CHECK(state.registers[4] == 6);
	
}
