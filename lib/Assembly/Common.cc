#include "Assembly/Common.h"

#include <array>

#include <utl/utility.hpp>

using namespace scatha;
using namespace Asm;

size_t Asm::sizeOf(ValueType type) {
    // clang-format off
    return UTL_MAP_ENUM(type, size_t, {
        { ValueType::RegisterIndex, 1 },
        { ValueType::MemoryAddress, 4 },
        { ValueType::Value8,        1 },
        { ValueType::Value16,       2 },
        { ValueType::Value32,       4 },
        { ValueType::Value64,       8 },
    });
    // clang-format on
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
    case 1: return ValueType::Value8;
    case 2: return ValueType::Value16;
    case 4: return ValueType::Value32;
    case 8: return ValueType::Value64;
    default: SC_UNREACHABLE();
    }
}

std::string_view Asm::toJumpInstName(CompareOperation condition) {
    return std::array{
#define SC_ASM_COMPARE_DEF(_0, _1, name) std::string_view(#name),
#include "Assembly/Lists.def"
    }[static_cast<size_t>(condition)];
}

std::string_view Asm::toSetInstName(CompareOperation condition) {
    return std::array{
#define SC_ASM_COMPARE_DEF(_0, name, _2) std::string_view(#name),
#include "Assembly/Lists.def"
    }[static_cast<size_t>(condition)];
}

std::string_view Asm::toString(UnaryArithmeticOperation operation) {
    return std::array{
#define SC_ASM_UNARY_ARITHMETIC_DEF(_, str) std::string_view(str),
#include "Assembly/Lists.def"
    }[static_cast<size_t>(operation)];
}

std::ostream& Asm::operator<<(std::ostream& ostream, UnaryArithmeticOperation operation) {
    return ostream << toString(operation);
}

std::string_view Asm::toString(ArithmeticOperation operation) {
    return std::array{
#define SC_ASM_ARITHMETIC_DEF(_, str) std::string_view(str),
#include "Assembly/Lists.def"
    }[static_cast<size_t>(operation)];
}

std::ostream& Asm::operator<<(std::ostream& ostream, ArithmeticOperation operation) {
    return ostream << toString(operation);
}
