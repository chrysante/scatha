#include "Assembly/Instruction.h"

using namespace scatha;
using namespace Asm;

void ArithmeticInst::verify() const {
    SC_ASSERT(operation() != ArithmeticOperation::_count, "Invalid operation.");
    SC_ASSERT(dest().is<RegisterIndex>(), "Dest operand must always be a register index.");
    SC_ASSERT(source().is<RegisterIndex>() || source().is<Value64>() || source().is<MemoryAddress>(),
              "Source operand must be either register index, value or memory address.");
}
