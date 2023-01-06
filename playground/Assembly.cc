#include "Assembly.h"

#include <iostream>

#include "Assembly/Assembler.h"
#include "Assembly/AssemblyStream.h"
#include "Assembly/Print.h"
#include "VM/Program.h"

using namespace scatha;
using namespace Asm;

void playground::testAsmModule() {
    AssemblyStream a;
    a.add(MoveInst(RegisterIndex(0), Value64(128), 8));     // a = 128
    a.add(AllocaInst(RegisterIndex(1), RegisterIndex(2)));  // ptr = alloca(...)
    a.add(MoveInst(MemoryAddress(1), RegisterIndex(0), 8)); // *ptr = a
    a.add(TerminateInst());

    std::cout << "\n=== Assembly ===\n\n";
    print(a);

    std::cout << "\n=== Program ===\n\n";

    auto program = Asm::assemble(a);
    print(program);
}
