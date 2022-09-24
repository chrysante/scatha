#include <iostream>

#include <utl/vector.hpp>

#include "Basic/Basic.h"
#include "AST/AST.h"
#include "Assembly/Assembler.h"
#include "Assembly/Assembly.h"
#include "VM/OpCode.h"
#include "VM/Program.h"
#include "VM/VirtualMachine.h"

using namespace scatha;
using namespace assembly;

int main() {
	
	using enum Instruction;
	
	Assembler a;
												                  // Main function
	a << allocReg << Value8(4);                                   // allocate 10 registers
	a << mov      << RegisterIndex(2) << Signed64(17);  // a = ...
	a << mov      << RegisterIndex(3) << Signed64(7);   // b = ...
	a << call     << Label(0) << Value8(2);
	a << callExt  << Value8(4) << Value8(0) << Value16(1);
	
	a << terminate;
	
	a << Label(0);                                                // gcd(i64 a, i64 b):
	a << allocReg << Value8(3);
	a << icmp     << RegisterIndex(1) << Value64(0);              // b == 0
	a << jne      << Label(1);
	a << mov      << RegisterIndex(2) << RegisterIndex(0);        // retval = a
	a << ret;
	a << Label(1);
	// swap a and b
	a << mov      << RegisterIndex(2) << RegisterIndex(1);        // c = b
	a << mov      << RegisterIndex(1) << RegisterIndex(0);        // b = a
	a << mov      << RegisterIndex(0) << RegisterIndex(2);        // a = c
	a << rem      << RegisterIndex(1) << RegisterIndex(0);
	
	a << jmp      << Label(0);                                   // tail call
	
//	print(a);
	
	std::cout << "\n\n-----------------------\n\n\n";
	
	vm::Program p = a.assemble();
	
	print(p);
	
	vm::VirtualMachine vm;
	
	vm.addExternalFunction(0, [](u64 value, vm::VirtualMachine*){ std::cout << value << std::endl; });
	vm.addExternalFunction(0, [](u64 value, vm::VirtualMachine*){ std::cout << utl::bit_cast<i64>(value) << std::endl; });
	vm.addExternalFunction(0, [](u64 value, vm::VirtualMachine*){ std::cout << utl::bit_cast<f64>(value) << std::endl; });
	
	vm.load(p);
	
	vm.execute();
	
	std::cout << std::endl;
	
}
