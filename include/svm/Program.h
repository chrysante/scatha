#ifndef SVM_PROGRAM_H_
#define SVM_PROGRAM_H_

#include <iosfwd>

#include <svm/Common.h>

namespace svm {

struct ProgramHeader {
    /// Arbitrary version string. Not yet sure what to put in here.
    u64 versionString[2];

    /// Size of the entire program including data and text section and this
    /// header.
    u64 size;

    /// Offset of the beginning of the data section.
    /// This should usually the size of the header.
    u64 dataOffset;

    /// Offset to the beginning of the text section.
    u64 textOffset;

    /// Position of the start/main function in the text section.
    u64 startAddress;
};

void print(u8 const* program);

void print(u8 const* program, std::ostream&);

} // namespace svm

#endif // SVM_PROGRAM_H_
