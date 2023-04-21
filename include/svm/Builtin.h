#ifndef SVM_BUILTIN_H_
#define SVM_BUILTIN_H_

#include <svm/ExternalFunction.h>

namespace svm {

/// Enum listing all builtin functions.
enum class Builtin {
#define SVM_BUILTIN_DEF(name, ...) name,
#include <svm/Builtin.def>
    _count
};

inline constexpr size_t BuiltinFunctionSlot = 0;

} // namespace svm

#endif // SVM_BUILTIN_H_
