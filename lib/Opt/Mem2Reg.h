#ifndef SCATHA_OPT_MEM2REG_H_
#define SCATHA_OPT_MEM2REG_H_

#include "Basic/Basic.h"

namespace scatha::ir {

class Context;
class Function;

} // namespace scatha::ir

namespace scatha::opt {

/// Perform memory to register promotion on \p function
/// \Returns True iff \p function was modified in the pass.
SCATHA(API) bool mem2Reg(ir::Context& context, ir::Function& function);

} // namespace scatha::opt

#endif // SCATHA_OPT_MEM2REG_H_
