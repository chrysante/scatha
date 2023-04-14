#ifndef SCATHA_CODEGEN_REORDERBLOCKS_H_
#define SCATHA_CODEGEN_REORDERBLOCKS_H_

#include "Basic/Basic.h"
#include "MIR/Fwd.h"

namespace scatha::cg {

SCATHA_TESTAPI void reorderBlocks(mir::Module& M);

} // namespace scatha::cg

#endif // SCATHA_CODEGEN_REORDERBLOCKS_H_
