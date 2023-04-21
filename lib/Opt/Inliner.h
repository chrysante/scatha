#ifndef SCATHA_OPT_INLINER_H_
#define SCATHA_OPT_INLINER_H_

#include "Common/Base.h"
#include "IR/Fwd.h"

namespace scatha::opt {

SCATHA_TESTAPI bool inlineFunctions(ir::Context& ctx, ir::Module& mod);

} // namespace scatha::opt

#endif // SCATHA_OPT_INLINER_H_
