#ifndef SCATHA_OPT_LOOPCANONICAL_H_
#define SCATHA_OPT_LOOPCANONICAL_H_

#include "Common/Base.h"
#include "IR/Fwd.h"

namespace scatha::opt {

SCATHA_TESTAPI bool makeLoopCanonical(ir::Context& ctx, ir::Function& F);

} // namespace scatha::opt

#endif // SCATHA_OPT_LOOPCANONICAL_H_
