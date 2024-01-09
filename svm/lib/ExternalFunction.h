#ifndef SVM_EXTERNALFUNCTION_H_
#define SVM_EXTERNALFUNCTION_H_

#include <array>
#include <cassert>
#include <concepts>
#include <string>

#include <ffi.h>
#include <utl/vector.hpp>

#include <svm/Common.h>

namespace svm {

class VirtualMachine;

/// Represents a function of the host application invocable by programs running
/// in the VM via the `cbltn` instruction.
struct ForeignFunction {
    using FuncPtr = void (*)();

    ForeignFunction() = default;

    /// We delete copy and move operations because we rely on address stability
    /// of the `values` array
    ForeignFunction(ForeignFunction const&) { assert(false); }

    ForeignFunction& operator=(ForeignFunction const&) { assert(false); }

    std::string name;
    FuncPtr funcPtr = nullptr;
    size_t numArgs;
    ffi_cif callInterface;
};

/// Represents a function of the host application invocable by programs running
/// in the VM via the `cbltn` instruction.
struct BuiltinFunction {
    using FuncPtr = void (*)(u64* regPtr, VirtualMachine* vm);

    BuiltinFunction() = default;

    BuiltinFunction(std::string name, FuncPtr funcPtr):
        _name(std::move(name)), funcPtr(funcPtr) {}

    BuiltinFunction(std::string name, std::convertible_to<FuncPtr> auto func):
        BuiltinFunction(std::move(name), (FuncPtr)func) {}

    /// \Returns the name of the function
    std::string const& name() const { return _name; }

    /// Invoke function
    void invoke(u64* regPtr, VirtualMachine* vm) const {
        assert(funcPtr);
        funcPtr(regPtr, vm);
    }

private:
    std::string _name;
    FuncPtr funcPtr = nullptr;
};

} // namespace svm

#endif // SVM_EXTERNALFUNCTION_H_
