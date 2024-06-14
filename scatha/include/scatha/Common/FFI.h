#ifndef SCATHA_COMMON_FFI_H_
#define SCATHA_COMMON_FFI_H_

/// This file provides common types and functions to work with foreign function
/// interfaces used across the compiler

#include <filesystem>
#include <span>
#include <string>

#include <utl/vector.hpp>

#include <scatha/Common/Base.h>

class FFITrivialType;

/// Base class of FFI types
class SCTEST_API FFIType {
public:
    /// This list is identical to the one in `<svm/Program.h>`
    /// This should never change but if it does both must be updated
    enum class Kind {
        Void,
        Int8,
        Int16,
        Int32,
        Int64,
        Float,
        Double,
        Pointer,
        Struct
    };

    virtual ~FFIType() = default;

    ///
    Kind kind() const { return _kind; }

    /// \Returns `true` if `kind()` is any of the non-struct types
    bool isTrivial() const {
        return (int)kind() >= (int)Kind::Void &&
               (int)kind() <= (int)Kind::Pointer;
    }

    /// Accessors for trivial types
    /// @{
    static FFIType const* Void();
    static FFIType const* Int8();
    static FFIType const* Int16();
    static FFIType const* Int32();
    static FFIType const* Int64();
    static FFIType const* Float();
    static FFIType const* Double();
    static FFIType const* Pointer();
    /// @}

    /// Factory for struct types
    /// Struct types are uniqued by their member types and have static lifetime
    static FFIType const* Struct(std::span<FFIType const* const> elementTypes);

protected:
    explicit FFIType(Kind kind): _kind(kind) {}

private:
    static FFITrivialType const _sVoid;
    static FFITrivialType const _sInt8;
    static FFITrivialType const _sInt16;
    static FFITrivialType const _sInt32;
    static FFITrivialType const _sInt64;
    static FFITrivialType const _sFloat;
    static FFITrivialType const _sDouble;
    static FFITrivialType const _sPointer;

    Kind _kind;
};

///
class FFITrivialType: public FFIType {
private:
    friend class FFIType;

    explicit FFITrivialType(Kind kind): FFIType(kind) {
        SC_EXPECT(kind != Kind::Struct);
    }
};

inline FFIType const* FFIType::Void() { return &_sVoid; }
inline FFIType const* FFIType::Int8() { return &_sInt8; }
inline FFIType const* FFIType::Int16() { return &_sInt16; }
inline FFIType const* FFIType::Int32() { return &_sInt32; }
inline FFIType const* FFIType::Int64() { return &_sInt64; }
inline FFIType const* FFIType::Float() { return &_sFloat; }
inline FFIType const* FFIType::Double() { return &_sDouble; }
inline FFIType const* FFIType::Pointer() { return &_sPointer; }

///
class FFIStructType: public FFIType {
public:
    /// Construct from element types
    FFIStructType(std::span<FFIType const* const> elementTypes);

    /// \Returns a view over the element types of this struct
    std::span<FFIType const* const> elements() const { return elems; }

private:
    utl::small_vector<FFIType const*> elems;
};

namespace scatha {

/// Represents the name and signature of a C function interface
class SCATHA_API ForeignFunctionInterface {
public:
    ///
    explicit ForeignFunctionInterface(
        std::string name, std::span<FFIType const* const> argumentTypes,
        FFIType const* returnType);

    /// The name of the function
    std::string const& name() const { return _name; }

    /// IDs of the argument types. We use `basic_string` for the small buffer
    /// optimization
    std::span<FFIType const* const> argumentTypes() const {
        return std::span{ sig }.subspan(1);
    }

    /// Return type ID
    FFIType const* returnType() const { return sig[0]; }

private:
    std::string _name;
    utl::small_vector<FFIType const*> sig;
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
