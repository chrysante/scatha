// PUBLIC-HEADER

#ifndef SVM_BUILTIN_H_
#define SVM_BUILTIN_H_

#include <utl/vector.hpp>

#include <svm/ExternalFunction.h>

namespace svm {

/// Enum listing all builtin functions.
enum class Builtin {
#define SVM_BUILTIN_DEF(name, ...) name,
#include <svm/Builtin.def>
    _count
};

inline constexpr size_t builtinFunctionSlot = 0;

utl::vector<ExternalFunction> makeBuiltinTable();

} // namespace svm

#endif // SVM_BUILTIN_H_
