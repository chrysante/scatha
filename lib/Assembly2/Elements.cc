#include "Assembly2/Elements.h"

using namespace scatha;
using namespace asm2;

void ArithmeticInst::verify() const {
    SC_ASSERT(isa<RegisterIndex>(lhs()), "Dest operand must always be a register index.");
    
    SC_ASSERT(isa<RegisterIndex>(rhs()) ||
              isa<Value64>(rhs())       ||
              isa<MemoryAddress>(rhs()), "Source operand must be either register index, value or memory address.");
}
