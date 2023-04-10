#ifndef SCATHA_CODEGEN_DEVIRTUALIZE_H_
#define SCATHA_CODEGEN_DEVIRTUALIZE_H_

#include "MIR/Fwd.h"

namespace scatha::cg {

SCATHA_TESTAPI bool devirtualize(mir::Function& F);

} // namespace scatha::cg

#endif // SCATHA_CODEGEN_DEVIRTUALIZE_H_
