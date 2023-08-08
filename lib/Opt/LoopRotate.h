#ifndef SCATHA_OPT_LOOPROTATE_H_
#define SCATHA_OPT_LOOPROTATE_H_

#include "IR/Fwd.h"

namespace scatha::opt {

bool rotateWhileLoops(ir::Context& context, ir::Function& function);

} // namespace scatha::opt

#endif // SCATHA_OPT_LOOPROTATE_H_
