#ifndef SCATHA_OPT_LOOPROTATE_H_
#define SCATHA_OPT_LOOPROTATE_H_

#include "IR/Fwd.h"

namespace scatha::opt {

/// This pass transform while loops into guarded do/while loops
SCATHA_TESTAPI bool rotateWhileLoops(ir::Context& context,
                                     ir::Function& function);

} // namespace scatha::opt

#endif // SCATHA_OPT_LOOPROTATE_H_
