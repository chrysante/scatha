#ifndef SCATHA_OPT_MEMTOREG_H_
#define SCATHA_OPT_MEMTOREG_H_

#include "Common/Base.h"

namespace scatha::ir {

class Context;
class Function;

} // namespace scatha::ir

namespace scatha::opt {

/// Perform memory to register promotion on \p function
/// \Returns True iff \p function was modified in the pass.
SCATHA_API bool memToReg(ir::Context& context, ir::Function& function);

} // namespace scatha::opt

#endif // SCATHA_OPT_MEMTOREG_H_
