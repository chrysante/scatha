#ifndef SCATHA_OPT_DCE_H_
#define SCATHA_OPT_DCE_H_

#include "Basic/Basic.h"

namespace scatha::ir {

class Context;
class Function;

} // namespace scatha::ir

namespace scatha::opt {

/// Eliminate dead code in \p function
/// \Returns True iff \p function was modified in the pass.
SCATHA_API bool dce(ir::Context& context, ir::Function& function);

} // namespace scatha::opt

#endif // SCATHA_OPT_DCE_H_
