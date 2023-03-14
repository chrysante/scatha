#ifndef SCATHA_OPT_CONSTANTPROPAGATION_H_
#define SCATHA_OPT_CONSTANTPROPAGATION_H_

#include "Basic/Basic.h"

namespace scatha::ir {

class Context;
class Function;

} // namespace scatha::ir

namespace scatha::opt {

/// Run sparse conditional constant propagation algorithm over \p function
/// Folds constants and eliminates dead code.
/// \returns `true` iff \p function was modified in the pass.
SCATHA(API)
bool propagateConstants(ir::Context& context, ir::Function& function);

} // namespace scatha::opt

#endif // SCATHA_OPT_CONSTANTPROPAGATION_H_
