#include "Assembly.h"

#include <iostream>

#include "Assembly2/AssemblyStream.h"
#include "Assembly2/Print.h"

using namespace scatha;
using namespace asm2;

void playground::testAsmModule() {
    AssemblyStream stream;
    stream.add(std::make_unique<MoveInst>(
        std::make_unique<RegisterIndex>(0),
        std::make_unique<Value64>(3)));
    
    stream.add(std::make_unique<StoreRegAddress>(
        std::make_unique<RegisterIndex>(1),
        std::make_unique<RegisterIndex>(2)));
    
    stream.add(std::make_unique<MoveInst>(
        std::make_unique<MemoryAddress>(1, 0, 0),
        std::make_unique<RegisterIndex>(0)));
    
    print(stream);
    
}
