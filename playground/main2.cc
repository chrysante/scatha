#include <iostream>

#include <utl/vector.hpp>

#include "Basic/Basic.h"
#include "AST/AST.h"
#include "Assembly/Assembler.h"
#include "Assembly/Assembly.h"
#include "Assembly/AssemblyStream.h"
#include "VM/OpCode.h"
#include "VM/Program.h"
#include "VM/VirtualMachine.h"

using namespace scatha;
using namespace assembly;

int main_() {
	
	using enum Instruction;
	
	AssemblyStream str;
                                                               // Main function
	str << allocReg << Value8(4);                              // allocate 10 registers
	str << mov      << RegisterIndex(2) << Signed64(17);       // a = ...
	str << mov      << RegisterIndex(3) << Signed64(7);        // b = ...
	str << call     << Label(0, 0) << Value8(2);
	str << callExt  << Value8(4) << Value8(0) << Value16(1);
	
	str << terminate;
	
	str << Label(0, 0);                                        // gcd(i64 a, i64 b):
	str << allocReg << Value8(3);
	str << icmp     << RegisterIndex(1) << Value64(0);         // b == 0
	str << jne      << Label(0, 1);
	str << mov      << RegisterIndex(2) << RegisterIndex(0);   // retval = a
	str << ret;
	str << Label(0, 1);
	// swap a and b
	str << mov      << RegisterIndex(2) << RegisterIndex(1);   // c = b
	str << mov      << RegisterIndex(1) << RegisterIndex(0);   // b = a
	str << mov      << RegisterIndex(0) << RegisterIndex(2);   // a = c
	str << rem      << RegisterIndex(1) << RegisterIndex(0);
	
	str << jmp      << Label(0, 0);                            // tail call

	Assembler a(str);
	
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
