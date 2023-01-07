#include "Assembly.h"

#include <iostream>

#include "Assembly/Assembler.h"
#include "Assembly/AssemblyStream.h"
#include "Assembly/Instruction.h"
#include "Assembly/Print.h"
#include "Assembly/Value.h"
#include "VM/Program.h"
#include "Common/Builtin.h"

using namespace scatha;
using namespace Asm;

void playground::testAsmModule() {
    AssemblyStream a;
    a.add(MoveInst(RegisterIndex(0), Value64(-1), 8));
    a.add(CallExtInst(/* regPtrOffset = */ 0,
                      builtinFunctionSlot,
                      /* functionIndex = */ static_cast<size_t>(Builtin::puti64)));
    a.add(MoveInst(RegisterIndex(0), Value64(' '), 8));
    a.add(CallExtInst(/* regPtrOffset = */ 0,
                      builtinFunctionSlot,
                      /* functionIndex = */ static_cast<size_t>(Builtin::putchar)));
    a.add(MoveInst(RegisterIndex(0), Value64('X'), 8));
    a.add(CallExtInst(/* regPtrOffset = */ 0,
                      builtinFunctionSlot,
                      /* functionIndex = */ static_cast<size_t>(Builtin::putchar)));
    a.add(MoveInst(RegisterIndex(0), Value64(' '), 8));
    a.add(CallExtInst(/* regPtrOffset = */ 0,
                      builtinFunctionSlot,
                      /* functionIndex = */ static_cast<size_t>(Builtin::putchar)));
    a.add(MoveInst(RegisterIndex(0), Value64(0.5), 8));
    a.add(CallExtInst(/* regPtrOffset = */ 0,
                      builtinFunctionSlot,
                      /* functionIndex = */ static_cast<size_t>(Builtin::putf64)));
    a.add(TerminateInst());

    std::cout << "\n=== Assembly ===\n\n";
    print(a);

    std::cout << "\n=== Program ===\n\n";

    auto program = Asm::assemble(a);
    print(program);
}
