#ifndef SCATHA_COMMON_BUILTIN_H_
#define SCATHA_COMMON_BUILTIN_H_

#include "Basic/Basic.h"

namespace scatha {

/// Enum listing all builtin functions.
enum class Builtin {
#define SC_BUILTIN_DEF(name, ...) name,
#include "Common/Builtin.def"
    _count
};

inline constexpr size_t builtinFunctionSlot = 0;

} // namespace scatha

#endif // SCATHA_COMMON_BUILTIN_H_
