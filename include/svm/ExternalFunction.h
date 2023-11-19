#ifndef SVM_EXTERNALFUNCTION_H_
#define SVM_EXTERNALFUNCTION_H_

#include <cassert>
#include <concepts>
#include <string>

#include <svm/Common.h>

namespace svm {

class VirtualMachine;

/// Represents a function of the host application invocable by programs running
/// in the VM via the `callext` instruction.
struct ExternalFunction {
    using FuncPtr = void (*)(u64* regPtr, VirtualMachine* vm, void* context);

    ExternalFunction() = default;

    ExternalFunction(std::string name,
                     FuncPtr funcPtr,
                     void* context = nullptr):
        _name(std::move(name)), funcPtr(funcPtr), ctx(context) {}

    ExternalFunction(std::string name, std::convertible_to<FuncPtr> auto func):
        _name(std::move(name)), funcPtr(func) {}

    /// \Returns the name of the function
    std::string const& name() const { return _name; }

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
    std::string _name;
    FuncPtr funcPtr = nullptr;
    void* ctx = nullptr;
};

} // namespace svm

#endif // SVM_EXTERNALFUNCTION_H_
