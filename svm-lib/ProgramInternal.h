#ifndef SVM_PROGRAMINTERNAL_H_
#define SVM_PROGRAMINTERNAL_H_

#include <iosfwd>

#include <utl/vector.hpp>

#include <svm/Common.h>
#include <svm/Program.h>

namespace svm {

class Program {
public:
    explicit Program(u8 const* data);
    
    utl::vector<u8> instructions;
    utl::vector<u8> data;
    size_t start = 0;
};

} // namespace svm

#endif // SVM_PROGRAMINTERNAL_H_
