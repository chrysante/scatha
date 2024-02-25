#ifndef SCATHA_CODEGEN_ISELCOMMON_H_
#define SCATHA_CODEGEN_ISELCOMMON_H_

#include "IR/CFG/Value.h"
#include "IR/Type.h"

namespace scatha::cg {

/// Target word size
inline constexpr size_t WordSize = 8;

/// \Returns the number of machine words required to store values of type \p
/// type
inline size_t numWords(ir::Type const& type) {
    return utl::ceil_divide(type.size(), WordSize);
}

/// \Returns the number of machine words required to store the value \p value
inline size_t numWords(ir::Value const& value) {
    return numWords(*value.type());
}

/// Used to calculate width of a slice when issuing multiple copy
/// instructions for large types.
inline size_t sliceWidth(size_t numBytes, size_t index, size_t numWords) {
    if (index != numWords - 1) {
        return 8;
    }
    size_t res = numBytes % 8;
    return res == 0 ? 8 : res;
}

} // namespace scatha::cg

#endif // SCATHA_CODEGEN_ISELCOMMON_H_
