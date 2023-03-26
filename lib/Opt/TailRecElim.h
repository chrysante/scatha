#ifndef SCATHA_OPT_TAILRECELIM_H_
#define SCATHA_OPT_TAILRECELIM_H_

#include "IR/Common.h"

namespace scatha::opt {

/// Tail recursion elimination.
///
/// \details
/// This pass replaces tail recursive calls with `goto`'s to the start of the
/// function, this creating loops.
///
SCATHA_TESTAPI bool tailRecElim(ir::Context& context, ir::Function& function);

} // namespace scatha::opt

#endif // SCATHA_OPT_TAILRECELIM_H_
