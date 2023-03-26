#ifndef SCATHA_IR_ATTRIBUTES_H_
#define SCATHA_IR_ATTRIBUTES_H_

#include <utl/common.hpp>

namespace scatha::ir {

enum class FunctionAttribute : unsigned {
    Memory_ReadNone  = 1 << 0,
    Memory_WriteNone = 1 << 1,
    Memory_None      = Memory_ReadNone | Memory_WriteNone
};

UTL_BITFIELD_OPERATORS(FunctionAttribute);

} // namespace scatha::ir

#endif // SCATHA_IR_ATTRIBUTES_H_
