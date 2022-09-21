#include <iostream>

#include <utl/vector.hpp>

#include "Basic/Basic.h"
#include "AST/AST.h"
#include "CodeGen/Assembler.h"
#include "VM/OpCode.h"
#include "VM/Program.h"
#include "VM/VirtualMachine.h"

using namespace scatha;
using namespace vm;
using namespace codegen;

/*
for (int i = 0; i < N; ++i) {
	<statements>;
}

--->>

int i = 0;
loop_begin:
if (!(i < 10)) {
	goto loop_end;
}
<statements>;
++i;
goto loop_begin;
loop_end:
*/

/*
                                             // ; Main function
 a << allocReg << u8(6);                     // ; allocate 10 registers
 a << movRV    << RV(0, 0);                  // ; i = 0
 a << movRV    << RV(1, 128.0);              // ; x = 8.0
 a << movMR    << MR(0, 0, 0, 1);            // ; memory[i + (0 << 0)] = x
 a << movRM    << RM(1, 0, 0, 0);            // ; x = memory[i + (0 << 0)]
 a << Label("loopBegin");                    // ; loop_begin:
 a << icmpRV   << RV(0, 16);                 // ; cmp(i, 100)
 a << jge      << makeLabel("loopEnd");      // ; if (i < 10) goto loop_end;

 // Loop body
 a << movRR    << RR(4, 1);
 a << movRV    << RV(5, f64(2));
 a << call     << makeLabel("f") << u8(4);
 a << movRR    << RR(1, 6);
 
 a << addRV    << RV(0, 1);                  // ; i += 1
 a << jmp      << makeLabel("loopBegin");    // ; goto loop_begin;
 
 a << Label("loopEnd");                      // ; loop_end:
 a << terminate;
 
 a << Label("f");                            // ; f(f64 a, f64 b):
 a << allocReg << u8(1);
 a << fdivRR   << RR(0, 1);                  // ; a /= b
 a << movRR    << RR(2, 0);                  // ; retval = a
 a << callExt  << u8(2) << u8(0) << u16(2);  // ; fprint(x)
 a << ret;
 */

int main() {
	
	
	
	using enum OpCode;
	
	Assembler a;
												// ; Main function
	a << allocReg << u8(4);                     // ; allocate 10 registers
	a << movRV    << RV(2, i64(151575450795));  // ; a = ...
	a << movRV    << RV(3, i64(55747384505));   // ; b = ...
	a << call     << makeLabel("gcd") << u8(2);
	a << callExt  << u8(4) << u8(0) << u16(1);
	
	a << terminate;
	
	a << Label("gcd");                          // ; gcd(i64 a, i64 b):
	a << allocReg << u8(3);
	a << icmpRV   << RV(1, i64(0));             // ; b == 0
	a << jne      << makeLabel("gcd-else");
	a << movRR    << RR(2, 0);                  // ; retval = a
	a << ret;
	a << Label("gcd-else");
	// swap a and b
	a << movRR    << RR(2, 1);                  // c = b
	a << movRR    << RR(1, 0);                  // b = a
	a << movRR    << RR(0, 2);                  // a = c
	a << remRR    << RR(1, 0);
	
	a << jmp      << makeLabel("gcd");          // tail call
	
	print(a);
	
	std::cout << "\n\n-----------------------\n\n\n";
	
	Program p = a.assemble();
	
	print(p);
	
	VirtualMachine vm;
	
	vm.addExternalFunction(0, [](u64 value, VirtualMachine*){ std::cout << value << std::endl; });
	vm.addExternalFunction(0, [](u64 value, VirtualMachine*){ std::cout << utl::bit_cast<i64>(value) << std::endl; });
	vm.addExternalFunction(0, [](u64 value, VirtualMachine*){ std::cout << utl::bit_cast<f64>(value) << std::endl; });
	
	vm.load(p);
	
	vm.execute();
	
	std::cout << std::endl;
	
}
