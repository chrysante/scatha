// PUBLIC-HEADER

#ifndef SVM_PROGRAM_H_
#define SVM_PROGRAM_H_

#include <utl/vector.hpp>

#include <svm/Common.h>

namespace svm {

struct ProgramHeader {
    /// Arbitrary version string. Not yet sure what to put in here.
    u64 versionString[2];
    /// Size of the entiry program including data and text section and this
    /// header.
    u64 size;
    /// Offset from the end of the header to beginning of data section.
    /// This should usually be 0.
    u64 dataOffset;
    /// Offset from the end of the header to beginning of text section.
    u64 textOffset;
    /// Position of the start/main function in the text section.
    u64 start;
};

void print(u8 const* program);

void print(u8 const* program, std::ostream&);

} // namespace svm

#endif // SVM_PROGRAM_H_
