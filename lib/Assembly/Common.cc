#include "Assembly/Common.h"

#include <array>

#include <utl/utility.hpp>

using namespace scatha;
using namespace Asm;

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
