#ifndef SCATHA_CODEGEN_TAILCALLOPT_H_
#define SCATHA_CODEGEN_TAILCALLOPT_H_

#include "Basic/Basic.h"
#include "MIR/Fwd.h"

namespace scatha::cg {

SCATHA_TESTAPI void tailCallOpt(mir::Function& F);

} // namespace scatha::cg

#endif // SCATHA_CODEGEN_TAILCALLOPT_H_
