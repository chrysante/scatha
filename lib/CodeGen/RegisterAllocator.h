#ifndef SCATHA_CG_REGISTERALLOCATOR_H_
#define SCATHA_CG_REGISTERALLOCATOR_H_

#include "Basic/Basic.h"
#include "MIR/Fwd.h"

namespace scatha::cg {

SCATHA_TESTAPI void allocateRegisters(mir::Function& F);

} // namespace scatha::cg

#endif // SCATHA_CG_REGISTERALLOCATOR_H_
