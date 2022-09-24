#define SC_TEST

#include <Catch/Catch2.hpp>

#include "Basic/Memory.h"
#include "Assembly/Assembler.h"
#include "VM/VirtualMachine.h"

using namespace scatha;
using namespace scatha::assembly;

TEST_CASE("Memory read write", "[assembly][vm]") {
	using enum Instruction;
	
	Assembler a;
	a << allocReg << Value8(4);                                  // allocate 4 registers
	a << mov      << RegisterIndex(0) << Unsigned64(128);        // a = 128
	a << setBrk   << Value8(0);                                  // allocate a bytes of memory
												                 // now a = <pointer-to-memory-section>
	a << mov << RegisterIndex(1)         << Signed64(-1);        // b = -1
	a << mov << MemoryAddress(0, 0, 0)   << RegisterIndex(1);    // memory[a] = b
	a << mov << RegisterIndex(2)         << Float64(1.5);        // c = 1.5
	a << mov << MemoryAddress(0, 8, 0)   << RegisterIndex(2);    // memory[a + 8] = c
	a << mov << RegisterIndex(2)         << Unsigned64(13);      // d = 13
	a << mov << MemoryAddress(0, 16, 0)  << RegisterIndex(2);    // memory[a + 16] = d
	
	a << terminate;
	
	vm::Program p = a.assemble();
	vm::VirtualMachine vm;
	vm.load(p);
	vm.execute();
	auto const& state = vm.getState();
	
	CHECK(read<i64>(state.memoryPtr) == -1);
	CHECK(read<f64>(state.memoryPtr + 8) == 1.5);
	CHECK(read<u64>(state.memoryPtr + 16) == 13);
}

TEST_CASE("Euclidean algorithm", "[vm][codegen]") {
	using enum Instruction;
	
	Assembler a;
	/*
	 should hold the result in the 4th register
	 */
												         // Main function
	a << allocReg << Value8(4);                          // allocate 4 registers
	a << mov    << RegisterIndex(2) << Signed64(54);     // a = 54
	a << mov    << RegisterIndex(3) << Signed64(24);     // b = 24
	a << call   << Label(0) << Value8(2);                // Label 0 == <gcd>

	a << terminate;

	a << Label(0);                                       // gcd(i64 a, i64 b):
	a << allocReg << Value8(3);
	a << icmp << RegisterIndex(1) << Signed64(0);        // b == 0
	a << jne << Label(1);
	a << mov << RegisterIndex(2) << RegisterIndex(0);    // retval = a
	a << ret;
	a << Label(1);                                       //abel 1 == <gcd-else>
	// swap a and b
	a << mov << RegisterIndex(2) << RegisterIndex(1);    // c = b
	a << mov << RegisterIndex(1) << RegisterIndex(0);    // b = a
	a << mov << RegisterIndex(0) << RegisterIndex(2);    // a = c
	a << rem << RegisterIndex(1) << RegisterIndex(0);

	a << jmp      << Label(0);                           // tail call

	vm::Program p = a.assemble();
	vm::VirtualMachine vm;
	vm.load(p);
	vm.execute();
	auto const& state = vm.getState();

	// gcd(54, 24) == 6
	CHECK(state.registers[4] == 6);

}
