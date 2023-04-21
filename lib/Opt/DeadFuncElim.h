#ifndef SCATHA_OPT_DEADFUNCELIM_H_
#define SCATHA_OPT_DEADFUNCELIM_H_

#include "Common/Base.h"
#include "IR/Fwd.h"

namespace scatha::opt {

/// Eliminate all function that do not get called by any externally visible
/// function
SCATHA_TESTAPI bool deadFuncElim(ir::Context& ctx, ir::Module& mod);

} // namespace scatha::opt

#endif // SCATHA_OPT_DEADFUNCELIM_H_
