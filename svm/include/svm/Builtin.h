#ifndef SVM_BUILTIN_H_
#define SVM_BUILTIN_H_

#include <cassert>
#include <string_view>

#include <svm/Common.h>

namespace svm {

/// Enum listing all builtin functions.
enum class Builtin {
#define SVM_BUILTIN_DEF(name, ...) name,
#include <svm/Builtin.def.h>
    _count
};

inline std::string_view toString(Builtin builtin) {
    switch (builtin) {
        // clang-format off
#define SVM_BUILTIN_DEF(name, ...) case Builtin::name: return #name;
#include <svm/Builtin.def.h>
        // clang-format on
    case Builtin::_count:
        unreachable();
    }
    unreachable();
}

inline bool isMemory(Builtin builtin) {
    return static_cast<size_t>(builtin) >=
               static_cast<size_t>(Builtin::memcpy) &&
           static_cast<size_t>(builtin) <=
               static_cast<size_t>(Builtin::dealloc);
}

inline constexpr size_t BuiltinFunctionSlot = 0;

} // namespace svm

#endif // SVM_BUILTIN_H_
