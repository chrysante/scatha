#ifndef SVM_PROGRAM_H_
#define SVM_PROGRAM_H_

#include <iosfwd>

#include <utl/vector.hpp>

#include <svm/Common.h>

namespace svm {

class Program {
public:
    friend void print(Program const&);
    friend void print(Program const&, std::ostream&);

    utl::vector<u8> instructions;
    utl::vector<u8> data;
    size_t start = 0;
};

} // namespace svm

#endif // SVM_PROGRAM_H_
