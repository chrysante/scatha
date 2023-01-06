#include "Assembly/Value.h"

using namespace scatha;
using namespace Asm;

Value Asm::promote(Value const& value, size_t size) {
    if (!isLiteralValue(value.valueType())) {
        return value;
    }
    size_t const valueSize = sizeOf(value.valueType());
    size = std::max(size, valueSize);
    switch (size) {
    case 1: return Value8(value->value());
    case 2: return Value16(value->value());
    case 4: return Value32(value->value());
    case 8: return Value64(value->value());
    default: SC_UNREACHABLE();
    }
}
