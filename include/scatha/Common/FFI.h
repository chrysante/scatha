#ifndef SCATHA_COMMON_FFI_H_
#define SCATHA_COMMON_FFI_H_

/// This file provides common types and functions to work with foreign function
/// interfaces used across the compiler

#include <filesystem>
#include <span>
#include <string>

#include <scatha/Common/Base.h>

/// This declaration is identical to the one in `<svm/Program.h>`
/// This should never change but if it does both must be updated
enum class FFIType : uint8_t {
    Void,
    Int8,
    Int16,
    Int32,
    Int64,
    Float,
    Double,
    Pointer,
    ArrayPointer,
};

namespace scatha {

/// Represents the name and signature of a C function interface
class SCATHA_API ForeignFunctionInterface {
public:
    ///
    explicit ForeignFunctionInterface(std::string name,
                                      std::span<FFIType const> argumentTypes,
                                      FFIType returnType);

    /// The name of the function
    std::string const& name() const { return _name; }

    /// IDs of the argument types. We use `basic_string` for the small buffer
    /// optimization
    std::span<FFIType const> argumentTypes() const {
        return std::span{ sig }.subspan(1);
    }

    /// Return type ID
    FFIType returnType() const { return sig[0]; }

private:
    std::string _name;
    std::basic_string<FFIType> sig;
};

/// Common representation of a foreign library, Used to communicate between sema
/// and the linker.
class SCATHA_API ForeignLibraryDecl {
public:
    explicit ForeignLibraryDecl(
        std::string name, std::optional<std::filesystem::path> resolvedPath):
        _name(std::move(name)), _path(resolvedPath) {}

    /// The (potentially nested) name
    std::string const& name() const { return _name; }

    /// The location of the library resolved by semantic analysis
    std::optional<std::filesystem::path> resolvedPath() const { return _path; }

private:
    std::string _name;
    std::optional<std::filesystem::path> _path;
};

} // namespace scatha

#endif // SCATHA_COMMON_FFI_H_
