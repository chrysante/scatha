#ifndef SCATHA_COMMON_FFI_H_
#define SCATHA_COMMON_FFI_H_

/// This file provides common types and functions to work with foreign function
/// interfaces used across the compiler

#include <span>
#include <string>

#include <svm/Program.h>

#include <scatha/Common/Base.h>

namespace scatha {

/// Represents the name and signature of a C function interface
class ForeignFunctionInterface {
public:
    ///
    explicit ForeignFunctionInterface(
        std::string name,
        std::span<svm::FFIType const> argumentTypes,
        svm::FFIType returnType);

    /// The name of the function
    std::string const& name() const { return _name; }

    /// IDs of the argument types. We use `basic_string` for the small buffer
    /// optimization
    std::span<svm::FFIType const> argumentTypes() const {
        return std::span{ sig }.subspan(1);
    }

    /// Return type ID
    svm::FFIType returnType() const { return sig[0]; }

private:
    std::string _name;
    std::basic_string<svm::FFIType> sig;
};

} // namespace scatha

#endif // SCATHA_COMMON_FFI_H_
