#ifndef SCATHA_CODEGEN_JUMPELISION_H_
#define SCATHA_CODEGEN_JUMPELISION_H_

#include "Basic/Basic.h"
#include "MIR/Fwd.h"

namespace scatha::cg {

SCATHA_TESTAPI void elideJumps(mir::Function& F);

} // namespace scatha::cg

#endif // SCATHA_CODEGEN_JUMPELISION_H_
