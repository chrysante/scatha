#ifndef SVM_PROGRAM_H_
#define SVM_PROGRAM_H_

#include <iosfwd>
#include <span>
#include <string>
#include <vector>

#include <svm/Common.h>

namespace svm {

/// Identifier that every program header version string must start with to
/// verify it is a correct program
inline constexpr u64 GlobalProgID = 0x5CBF;

inline constexpr u64 InvalidAddress = ~u64(0);

///
struct ProgramHeader {
    /// Arbitrary version string. Not yet sure what to put in here.
    u64 versionString[2];

    /// Size of the entire program including data and text section and this
    /// header.
    u64 size;

    /// Position of the start/main function in the text section.
    u64 startAddress;

    /// Offset of the beginning of the data section.
    /// This should usually the size of the header.
    u64 dataOffset;

    /// Offset to the beginning of the text section.
    u64 textOffset;

    /// Offset to a list of dynamic library and FFI declarations
    u64 FFIDeclOffset;
};

/// The FFI decl format is as follows:
/// ```
///  library-list -> u32        // Number of foreign libraries
///                  [lib-decl] // List of library declarations
///  lib-decl     -> [char]\0   // Null-terminated string denoting library name
///                  u32        // Number of foreign function declarations
///                  [ffi-decl] // List of function declarations
///  ffi-decl     -> [char]\0   // Null-terminated string denoting function name
///                  u8         // Number of arguments
///                  [ffi-type] // List of argument types
///                  ffi-type   // Return type
///                  u32        // Index
///  ffi-type     -> u8         // Trivial type ID
///                | u8         // Struct type ID
///                  u16        // Number of elements
///                  [ffi-type] // Element types
/// ```

class FFITrivialType;

class FFIType {
public:
    /// This declaration is identical to the one in `<scatha/Common/FFI.h>`
    /// This should never change but if it does both must be updated
    enum class Kind : uint8_t {
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

    static FFIType const* Void();
    static FFIType const* Int8();
    static FFIType const* Int16();
    static FFIType const* Int32();
    static FFIType const* Int64();
    static FFIType const* Float();
    static FFIType const* Double();
    static FFIType const* Pointer();
    static FFIType const* Trivial(Kind kind);
    static FFIType const* Struct(std::span<FFIType const* const> elementTypes);

    ///
    Kind kind() const { return _kind; }

    /// \Returns `true` if `kind()` is any of the non-struct types
    static bool isTrivial(Kind kind) {
        return (int)kind >= (int)Kind::Void && (int)kind <= (int)Kind::Pointer;
    }

    ///  \overload
    bool isTrivial() const { return isTrivial(kind()); }

protected:
    explicit FFIType(Kind kind): _kind(kind) {}

private:
    static FFITrivialType _sVoid;
    static FFITrivialType _sInt8;
    static FFITrivialType _sInt16;
    static FFITrivialType _sInt32;
    static FFITrivialType _sInt64;
    static FFITrivialType _sFloat;
    static FFITrivialType _sDouble;
    static FFITrivialType _sPointer;

    Kind _kind;
};

class FFITrivialType: public FFIType {
public:
    explicit FFITrivialType(Kind kind): FFIType(kind) {}
};

class FFIStructType: public FFIType {
public:
    explicit FFIStructType(std::vector<FFIType const*> types):
        FFIType(Kind::Struct), elems(std::move(types)) {}

    std::span<FFIType const* const> elements() const { return elems; }

private:
    std::vector<FFIType const*> elems;
};

inline FFIType const* FFIType::Void() { return &_sVoid; }
inline FFIType const* FFIType::Int8() { return &_sInt8; }
inline FFIType const* FFIType::Int16() { return &_sInt16; }
inline FFIType const* FFIType::Int32() { return &_sInt32; }
inline FFIType const* FFIType::Int64() { return &_sInt64; }
inline FFIType const* FFIType::Float() { return &_sFloat; }
inline FFIType const* FFIType::Double() { return &_sDouble; }
inline FFIType const* FFIType::Pointer() { return &_sPointer; }

/// Foreign function metadata
struct FFIDecl {
    std::string name;
    std::vector<FFIType const*> argumentTypes;
    FFIType const* returnType;
    size_t index;
    void* ptr;
};

/// Metadata of a library dependency
struct FFILibDecl {
    std::string name;
    std::vector<FFIDecl> funcDecls;
};

///
class ProgramView {
public:
    explicit ProgramView(u8 const* data);

    ///
    ProgramHeader header;

    /// Address of the 'start' label
    u64 startAddress = InvalidAddress;

    /// View over the entire binary section of the program, i.e. `data` and
    /// `text` adjacently combined
    std::span<u8 const> binary;

    /// View over the static data section of the program
    std::span<u8 const> data;

    /// View over the code of the program
    std::span<u8 const> text;

    ///
    std::vector<FFILibDecl> libDecls;
};

///
void print(u8 const* program);

///
void print(u8 const* program, std::ostream&);

} // namespace svm

#endif // SVM_PROGRAM_H_
