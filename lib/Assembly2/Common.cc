#include "Assembly2/Common.h"

#include <array>

#include <utl/utility.hpp>

using namespace scatha;
using namespace asm2;

std::string_view asm2::toJumpInstName(CompareOperation condition) {
    return std::array{
#define SC_ASM_COMPARE_DEF(_0, _1, name) std::string_view(#name),
#include "Assembly2/Elements.def"
    }[static_cast<size_t>(condition)];
}

std::string_view asm2::toSetInstName(CompareOperation condition) {
    return std::array{
#define SC_ASM_COMPARE_DEF(_0, name, _2) std::string_view(#name),
#include "Assembly2/Elements.def"
    }[static_cast<size_t>(condition)];
}

std::string_view asm2::toString(ArithmeticOperation operation) {
    return std::array{
#define SC_ASM_ARITHMETIC_DEF(_, str) std::string_view(str),
#include "Assembly2/Elements.def"
    }[static_cast<size_t>(operation)];
}

std::ostream& asm2::operator<<(std::ostream& ostream, ArithmeticOperation operation) {
    return ostream << toString(operation);
}
