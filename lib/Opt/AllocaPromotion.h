#ifndef SCATHA_OPT_ALLOCAPROMOTION_H_
#define SCATHA_OPT_ALLOCAPROMOTION_H_

#include "IR/Fwd.h"

namespace scatha::opt {

///
///
///
bool isPromotable(ir::Alloca const* inst);

///
///
///
void promoteAlloca(ir::Alloca* inst,
                   ir::Context& ctx,
                   ir::DominanceInfo const& domInfo);

} // namespace scatha::opt

#endif // SCATHA_OPT_ALLOCAPROMOTION_H_
