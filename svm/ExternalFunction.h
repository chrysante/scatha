#ifndef SVM_EXTERNALFUNCTION_H_
#define SVM_EXTERNALFUNCTION_H_

#include <svm/Common.h>

namespace svm {

class VirtualMachine;

struct ExternalFunction {
    using FuncPtr = void (*)(u64* regPtr, VirtualMachine* vm, void* context);

    ExternalFunction() = default;

    ExternalFunction(FuncPtr funcPtr, void* context = nullptr):
        funcPtr(funcPtr), context(context) {}

    FuncPtr funcPtr = nullptr;
    void* context   = nullptr;
};

} // namespace svm

#endif // SVM_EXTERNALFUNCTION_H_
