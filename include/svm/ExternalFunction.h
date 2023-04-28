#ifndef SVM_EXTERNALFUNCTION_H_
#define SVM_EXTERNALFUNCTION_H_

#include <cassert>
#include <concepts>

#include <svm/Common.h>

namespace svm {

class VirtualMachine;

/// Represents a function of the host application invocable by programs running
/// in the VM via the `callext` instruction.
struct ExternalFunction {
    using FuncPtr = void (*)(u64* regPtr, VirtualMachine* vm, void* context);

    ExternalFunction() = default;

    ExternalFunction(FuncPtr funcPtr, void* context = nullptr):
        funcPtr(funcPtr), ctx(context) {}

    ExternalFunction(std::convertible_to<FuncPtr> auto func): funcPtr(func) {}

    /// Invoke function
    void invoke(u64* regPtr, VirtualMachine* vm) const {
        assert(funcPtr);
        funcPtr(regPtr, vm, ctx);
    }

    /// \Returns The context pointer
    void* context() { return ctx; }

    /// \overload
    void const* context() const { return ctx; }

private:
    FuncPtr funcPtr = nullptr;
    void* ctx       = nullptr;
};

} // namespace svm

#endif // SVM_EXTERNALFUNCTION_H_
