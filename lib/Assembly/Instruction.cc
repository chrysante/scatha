#include "Assembly/Instruction.h"

using namespace scatha;
using namespace Asm;

void MoveInst::verify() {
    SC_ASSERT(numBytes() == 1 || numBytes() == 2 || numBytes() == 4 ||
                  numBytes() == 8,
              "Invalid numer of bytes.");
}

void CMoveInst::verify() {
    SC_ASSERT(numBytes() == 1 || numBytes() == 2 || numBytes() == 4 ||
                  numBytes() == 8,
              "Invalid numer of bytes.");
}

void ArithmeticInst::verify() const {
    SC_ASSERT(operation() != ArithmeticOperation::_count, "Invalid operation.");
    SC_ASSERT(dest().is<RegisterIndex>(),
              "Dest operand must always be a register index.");
    SC_ASSERT(width() == 4 || width() == 8, "Invalid width");
    if (isShift(operation())) {
        /// Shift operations require literal values to be 8 bits wide
        SC_ASSERT(!source().is<Value16>(), "Invalid operand");
        SC_ASSERT(!source().is<Value32>(), "Invalid operand");
        SC_ASSERT(!source().is<Value64>(), "Invalid operand");
    }
    else {
        if (width() == 4) {
            SC_ASSERT(!source().is<Value8>(), "Invalid operand");
            SC_ASSERT(!source().is<Value16>(), "Invalid operand");
            SC_ASSERT(!source().is<Value64>(), "Invalid operand");
        }
        if (width() == 8) {
            SC_ASSERT(!source().is<Value8>(), "Invalid operand");
            SC_ASSERT(!source().is<Value16>(), "Invalid operand");
            SC_ASSERT(!source().is<Value32>(), "Invalid operand");
        }
    }
}

void ConvInst::verify() {
    switch (_type) {
    case Type::Signed:
        SC_ASSERT(fromBits() == 1 || fromBits() == 8 || fromBits() == 16 ||
                      fromBits() == 32 || fromBits() == 64,
                  "Invalid source operand bit width");
        break;
    case Type::Float:
        SC_ASSERT(fromBits() == 32 || fromBits() == 64,
                  "Invalid source operand bit width");
        break;

    default:
        SC_DEBUGFAIL();
    }
}
