#ifndef SCATHA_CODEGEN_DEADCODEELIM_H_
#define SCATHA_CODEGEN_DEADCODEELIM_H_

#include "MIR/Fwd.h"

namespace scatha::cg {

SCATHA_TESTAPI bool deadCodeElim(mir::Function& F);

} // namespace scatha::cg

#endif // SCATHA_CODEGEN_DEADCODEELIM_H_
