#ifndef SCATHA_OPT_ALLOCAPROMOTION_H_
#define SCATHA_OPT_ALLOCAPROMOTION_H_

#include "IR/Fwd.h"

namespace scatha::opt {

/// FIXME: We could proably speed up the promotion algorithm if we batch
/// promotions and promote multiple allocas at the same time

/// Checks whether \p allocaInst is promotable to SSA form
bool isPromotable(ir::Alloca const* allocaInst);

/// Promote \p allocaInst to SSA form.
/// \pre Calls to this function must be guarded with a call to `isPromotable()`.
/// This function will trap if \p allocaInst is not promotable
void promoteAlloca(ir::Alloca* allocaInst, ir::Context& ctx,
                   ir::DominanceInfo const& domInfo);

/// Try to romote \p allocaInst to SSA form.
/// \Return `true` if the promotion was successful.
/// No changes are made to the function if promotion was not successful
bool tryPromoteAlloca(ir::Alloca* allocaInst, ir::Context& ctx,
                      ir::DominanceInfo const& domInfo);

} // namespace scatha::opt

#endif // SCATHA_OPT_ALLOCAPROMOTION_H_
