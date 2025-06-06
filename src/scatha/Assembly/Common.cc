#include "Assembly/Common.h"

#include <array>
#include <ostream>

#include <utl/utility.hpp>

using namespace scatha;
using namespace Asm;

size_t Asm::sizeOf(ValueType type) {
    switch (type) {
    case ValueType::RegisterIndex:
        return 1;
    case ValueType::MemoryAddress:
        return 4;
    case ValueType::Value8:
        return 1;
    case ValueType::Value16:
        return 2;
    case ValueType::Value32:
        return 4;
    case ValueType::Value64:
        return 8;
    case ValueType::LabelPosition:
        return 4;
    }
}

bool Asm::isLiteralValue(ValueType type) {
    return utl::to_underlying(type) >= utl::to_underlying(ValueType::Value8) &&
           utl::to_underlying(type) <= utl::to_underlying(ValueType::Value64);
}

ValueType Asm::promote(ValueType type, size_t size) {
    if (!isLiteralValue(type)) {
        return type;
    }
    switch (std::max(size, sizeOf(type))) {
    case 1:
        return ValueType::Value8;
    case 2:
        return ValueType::Value16;
    case 4:
        return ValueType::Value32;
    case 8:
        return ValueType::Value64;
    default:
        SC_UNREACHABLE();
    }
}

std::string_view Asm::toCMoveInstName(CompareOperation condition) {
    return std::array{
#define SC_ASM_COMPARE_DEF(_0, name) std::string_view("cmov" name),
#include "Assembly/Lists.def.h"
    }[static_cast<size_t>(condition)];
}

std::string_view Asm::toJumpInstName(CompareOperation condition) {
    return std::array{
#define SC_ASM_COMPARE_DEF(JUMP, name)                                         \
    CompareOperation::JUMP == CompareOperation::None ?                         \
        std::string_view("jmp" name) :                                         \
        std::string_view("j" name),
#include "Assembly/Lists.def.h"
    }[static_cast<size_t>(condition)];
}

std::string_view Asm::toSetInstName(CompareOperation condition) {
    return std::array{
#define SC_ASM_COMPARE_DEF(_0, name) std::string_view("set" name),
#include "Assembly/Lists.def.h"
    }[static_cast<size_t>(condition)];
}

std::string_view Asm::toString(UnaryArithmeticOperation operation) {
    return std::array{
#define SC_ASM_UNARY_ARITHMETIC_DEF(_, str) std::string_view(str),
#include "Assembly/Lists.def.h"
    }[static_cast<size_t>(operation)];
}

std::ostream& Asm::operator<<(std::ostream& ostream,
                              UnaryArithmeticOperation operation) {
    return ostream << toString(operation);
}

std::string_view Asm::toString(ArithmeticOperation operation) {
    return std::array{
#define SC_ASM_ARITHMETIC_DEF(_, str) std::string_view(str),
#include "Assembly/Lists.def.h"
    }[static_cast<size_t>(operation)];
}

std::ostream& Asm::operator<<(std::ostream& ostream,
                              ArithmeticOperation operation) {
    return ostream << toString(operation);
}

bool Asm::isShift(ArithmeticOperation op) {
    return op == ArithmeticOperation::LShL || op == ArithmeticOperation::LShR ||
           op == ArithmeticOperation::AShL || op == ArithmeticOperation::AShR;
}
