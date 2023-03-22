#ifndef SCATHA_OPT_INLINER_H_
#define SCATHA_OPT_INLINER_H_

#include "Basic/Basic.h"
#include "IR/Common.h"

namespace scatha::opt {

SCATHA_TESTAPI bool inlineFunctions(ir::Context& ctx, ir::Module& mod);

} // namespace scatha::opt

#endif // SCATHA_OPT_INLINER_H_
