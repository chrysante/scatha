#ifndef SVM_PROGRAMINTERNAL_H_
#define SVM_PROGRAMINTERNAL_H_

#include <span>

#include <svm/Common.h>
#include <svm/Program.h>

namespace svm {

///
class ProgramView {
public:
    explicit ProgramView(u8 const* data);

    /// View over the entire binary section of the program, i.e. `data` and
    /// `text` adjacently combined
    std::span<u8 const> binary;

    /// View over the static data section of the program
    std::span<u8 const> data;

    /// View over the code of the program
    std::span<u8 const> text;

    /// Address of the 'start' label
    size_t startAddress = 0;
};

} // namespace svm

#endif // SVM_PROGRAMINTERNAL_H_
