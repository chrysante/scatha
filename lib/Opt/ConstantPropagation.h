#ifndef SCATHA_OPT_CONSTANTPROPAGATION_H_
#define SCATHA_OPT_CONSTANTPROPAGATION_H_

#include "Basic/Basic.h"

namespace scatha::ir {

class Context;
class Module;

} // namespace scatha::ir

namespace scatha::opt {

/// Run sparse conditional constant propagation algorithm over \p mod
/// Folds constants and eliminates dead code.
SCATHA(API) void propagateConstants(ir::Context& context, ir::Module& mod);

} // namespace scatha::opt

#endif // SCATHA_OPT_CONSTANTPROPAGATION_H_
