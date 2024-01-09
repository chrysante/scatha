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
    ForeignFunction(ForeignFunction const&) { unreachable(); }

    ForeignFunction& operator=(ForeignFunction const&) { unreachable(); }

    std::string name;
    FuncPtr funcPtr = nullptr;
    ffi_cif callInterface;
    utl::small_vector<ffi_type*> argumentTypes;
    utl::small_vector<void*> arguments;
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
