#ifndef SVM_VMDATA_H_
#define SVM_VMDATA_H_

#include <svm/Common.h>
#include <svm/VirtualPointer.h>

namespace svm {

/// Internal VM flags that store the result of the last compare instruction
struct CompareFlags {
    bool less  : 1;
    bool equal : 1;
};

/// Represents the state of an invocation of the virtual machine.
struct ExecutionFrame {
    u64* regPtr = nullptr;
    u64* bottomReg = nullptr;
    u8 const* iptr = nullptr;
    VirtualPointer stackPtr{};
};

///
struct VMStats {
    size_t executedInstructions = 0;
};

} // namespace svm

#endif // SVM_VMDATA_H_
