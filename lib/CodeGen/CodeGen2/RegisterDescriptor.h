#ifndef SCATHA_CG_REGISTERDESCRIPTOR_H_
#define SCATHA_CG_REGISTERDESCRIPTOR_H_

#include <utl/vector.hpp>

#include "IR/Common.h"

namespace scatha::cg {

class RegisterDescriptor {
public:
    explicit RegisterDescriptor(size_t numRegisters);

private:
    utl::vector<ir::Instruction const*> registers;
};

} // namespace scatha::cg

#endif // SCATHA_CG_REGISTERDESCRIPTOR_H_
