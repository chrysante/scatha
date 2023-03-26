// SCATHA-PUBLIC-HEADER

#ifndef SCATHA_SEMA_ATTRIBUTES_H_
#define SCATHA_SEMA_ATTRIBUTES_H_

#include <utl/common.hpp>

namespace scatha::sema {

enum class FunctionAttribute : unsigned {
    None  = 0,
    All   = unsigned(-1),
    Const = 1 << 0,
    Pure  = 1 << 1
};

UTL_BITFIELD_OPERATORS(FunctionAttribute);

} // namespace scatha::sema

#endif // SCATHA_SEMA_ATTRIBUTES_H_
