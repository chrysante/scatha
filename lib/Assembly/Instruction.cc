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

static void verifyWidth(Type type, size_t bits) {
    switch (type) {
    case Type::Signed:
        [[fallthrough]];
    case Type::Unsigned:
        SC_ASSERT(bits == 8 || bits == 16 || bits == 32 || bits == 64,
                  "Invalid bit width");
        break;
    case Type::Float:
        SC_ASSERT(bits == 32 || bits == 64, "Invalid bit width");
        break;

    case Type::_count:
        SC_UNREACHABLE();
    }
};

void TruncExtInst::verify() { verifyWidth(type(), fromBits()); }

void ConvertInst::verify() {
    verifyWidth(fromType(), fromBits());
    verifyWidth(toType(), toBits());
}
