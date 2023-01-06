#include "Assembly/Instruction.h"

using namespace scatha;
using namespace Asm;

void MoveInst::verify() {
    SC_ASSERT(_numBytes == 1 ||
              _numBytes == 2 ||
              _numBytes == 4 ||
              _numBytes == 8, "Invalid numer of bytes.");
}

void ArithmeticInst::verify() const {
    SC_ASSERT(operation() != ArithmeticOperation::_count, "Invalid operation.");
    SC_ASSERT(dest().is<RegisterIndex>(), "Dest operand must always be a register index.");
}
