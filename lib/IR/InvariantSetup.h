#ifndef SCATHA_IR_INVARIANTSETUP_H_
#define SCATHA_IR_INVARIANTSETUP_H_

#include "Common/Base.h"
#include "IR/Fwd.h"

namespace scatha::ir {

/// Sets up several IR invariants of \p function
/// In particular for all blocks
/// - all instruction past the first terminator are erased
/// - a return instruction is appended if the block does not have a terminator
/// - the predecessor relationship is setup according to the terminator
SCATHA_API void setupInvariants(Context& ctx, Function& function);

} // namespace scatha::ir

#endif // SCATHA_IR_INVARIANTSETUP_H_
