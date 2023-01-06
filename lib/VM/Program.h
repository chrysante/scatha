// SCATHA-PUBLIC-HEADER

#ifndef SCATHA_VM_PROGRAM_H_
#define SCATHA_VM_PROGRAM_H_

#include <iosfwd>

#include <utl/vector.hpp>

#include <scatha/Basic/Basic.h>

namespace scatha::vm {

class Program {
public:
    SCATHA(API) friend void print(Program const&);
    SCATHA(API) friend void print(Program const&, std::ostream&);

    utl::vector<u8> instructions;
    utl::vector<u8> data;
    size_t start = 0;
};

} // namespace scatha::vm

#endif // SCATHA_VM_PROGRAM_H_
