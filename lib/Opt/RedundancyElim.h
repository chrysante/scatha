#ifndef SCATHA_OPT_REDUNDANCYELIM_H_
#define SCATHA_OPT_REDUNDANCYELIM_H_

#include "Common/Base.h"
#include "IR/Fwd.h"

namespace scatha::opt {

SCATHA_API bool redundancyElim(ir::Context& context, ir::Function& function);

} // namespace scatha::opt

#endif // SCATHA_OPT_REDUNDANCYELIM_H_
