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
	a << setBrk   << RegisterIndex(0);                           // allocate a bytes of memory
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

static void testArithmeticRR(Instruction i, auto arg1, auto arg2, auto reference, Value64::Type type) {
	using enum Instruction;
	
	Assembler a;
	a << allocReg << Value8(2);
	a << mov      << RegisterIndex(0) << Value64(utl::bit_cast<u64>(arg1), type);
	a << mov      << RegisterIndex(1) << Value64(utl::bit_cast<u64>(arg2), type);
	a << i        << RegisterIndex(0) << RegisterIndex(1);
	a << terminate;
	
	vm::Program p = a.assemble();
	vm::VirtualMachine vm;
	vm.load(p);
	vm.execute();
	auto const& state = vm.getState();
	
	CHECK(read<decltype(reference)>(state.regPtr) == reference);
}

static void testArithmeticRV(Instruction i, auto arg1, auto arg2, auto reference, Value64::Type type) {
	using enum Instruction;
	
	Assembler a;
	a << allocReg << Value8(1);
	a << mov      << RegisterIndex(0) << Value64(utl::bit_cast<u64>(arg1), type);
	a << i        << RegisterIndex(0) << Value64(utl::bit_cast<u64>(arg2), type);
	a << terminate;
	
	vm::Program p = a.assemble();
	vm::VirtualMachine vm;
	vm.load(p);
	vm.execute();
	auto const& state = vm.getState();
	
	CHECK(read<decltype(reference)>(state.regPtr) == reference);
}

static void testArithmeticRM(Instruction i, auto arg1, auto arg2, auto reference, Value64::Type type) {
	using enum Instruction;

	Assembler a;
	a << allocReg << Value8(3);
	a << mov      << RegisterIndex(0) << Value64(utl::bit_cast<u64>(arg1), type);
	a << mov      << RegisterIndex(1) << Unsigned64(8);                           // R[1] = 8
	a << setBrk   << RegisterIndex(1);                                            // allocate R[1] bytes of memory
																                  // now R[1] = <pointer-to-memory-section>
	a << mov      << RegisterIndex(2) << Value64(utl::bit_cast<u64>(arg2), type); // R[2] = arg2
	a << mov      << MemoryAddress(1, 0, 0) << RegisterIndex(2);
	a << i        << RegisterIndex(0) << MemoryAddress(1, 0, 0);
	a << terminate;

	vm::Program p = a.assemble();
	vm::VirtualMachine vm;
	vm.load(p);
	vm.execute();
	auto const& state = vm.getState();

	CHECK(read<decltype(reference)>(state.regPtr) == reference);
}

static void testArithmetic(Instruction i, auto arg1, auto arg2, auto reference, Value64::Type type) {
	testArithmeticRR(i, arg1, arg2, reference, type);
	testArithmeticRV(i, arg1, arg2, reference, type);
	testArithmeticRM(i, arg1, arg2, reference, type);
}

TEST_CASE("Arithmetic", "[assembly][vm]") {
	using enum Instruction;

	SECTION("add") {
		testArithmetic(add, u64(6), u64( 2), u64( 8), Value64::UnsignedIntegral);
		testArithmetic(add, i64(2), i64(-6), i64(-4), Value64::SignedIntegral);
	}
	SECTION("sub") {
		testArithmetic(sub, u64(6), u64( 2), u64(4), Value64::UnsignedIntegral);
		testArithmetic(sub, i64(2), i64(-6), i64(8), Value64::SignedIntegral);
	}
	SECTION("mul") {
		testArithmetic(mul, u64(6), u64( 2), u64( 12), Value64::UnsignedIntegral);
		testArithmetic(mul, i64(2), i64(-6), i64(-12), Value64::SignedIntegral);
	}
	SECTION("div") {
		testArithmetic(div, u64(  6), u64(2), u64( 3), Value64::UnsignedIntegral);
		testArithmetic(div, u64(100), u64(3), u64(33), Value64::UnsignedIntegral);
	}
	SECTION("idiv") {
		testArithmetic(idiv, i64(  6), i64(-2), i64( -3), Value64::SignedIntegral);
		testArithmetic(idiv, i64(100), i64(-3), i64(-33), Value64::SignedIntegral);
	}
	SECTION("rem") {
		testArithmetic(rem, u64(  6), u64(2), u64(0), Value64::UnsignedIntegral);
		testArithmetic(rem, u64(100), u64(3), u64(1), Value64::UnsignedIntegral);
	}
	SECTION("irem") {
		testArithmetic(irem, i64(   6), i64(-2), i64( 0), Value64::SignedIntegral);
		testArithmetic(irem, i64( 100), i64(-3), i64( 1), Value64::SignedIntegral);
		testArithmetic(irem, i64(-100), i64( 3), i64(-1), Value64::SignedIntegral);
	}
	SECTION("fadd") {
		testArithmetic(fadd, f64(6.4), f64(-2.2), f64(4.2), Value64::FloatingPoint);
	}
	SECTION("fsub") {
		testArithmetic(fsub, f64(6), f64(2.3), f64(3.7), Value64::FloatingPoint);
	}
	SECTION("fmul") {
		testArithmetic(fmul, f64(2.4), f64(2.5), f64(6.0), Value64::FloatingPoint);
	}
	SECTION("fdiv") {
		testArithmetic(fdiv, f64(6.3), f64(3.0), f64(2.1), Value64::FloatingPoint);
	}
}
