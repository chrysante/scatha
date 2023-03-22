#ifndef SCATHA_OPT_SIMPLIFYCFG_H_
#define SCATHA_OPT_SIMPLIFYCFG_H_

#include "Basic/Basic.h"
#include "IR/Common.h"

namespace scatha::opt {

SCATHA_TESTAPI bool simplifyCFG(ir::Context& ctx, ir::Function& function);

} // namespace scatha::opt

#endif // SCATHA_OPT_SIMPLIFYCFG_H_
