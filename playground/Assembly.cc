#include "Assembly.h"

#include <iostream>

#include "Assembly2/AssemblyStream.h"
#include "Assembly2/Print.h"

using namespace scatha;
using namespace asm2;

void playground::testAsmModule() {
    AssemblyStream stream;
    stream.add(MoveInst(RegisterIndex(0), Value64(3)));
    stream.add(AllocaInst(RegisterIndex(1), RegisterIndex(2)));
    stream.add(MoveInst(MemoryAddress(1, 0, 0), RegisterIndex(0)));
    print(stream);
    
}
