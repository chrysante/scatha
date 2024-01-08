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
inline u64 const GlobalProgID = 0x5CBF;

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
/// - `u32` ; Number of foreign libraries
/// - `[lib-decl]` ; List of library declarations where `lib-decl` is:
///   - `[char]\0` Null-terminated string denoting library name
///   - `u32` ; Number of foreign function declarations
///   - `[ffi-decl]` ; List of function declarations where `ffi-decl` is:
///     - `[char]\0` ; Null-terminated string denoting the name
///     - `u32` ; Slot
///     - `u32` ; Index

struct FFIDecl {
    std::string name;
    size_t slot;
    size_t index;
};

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
    size_t startAddress = 0;

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
