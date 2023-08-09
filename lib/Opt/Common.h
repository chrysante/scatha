#ifndef SCATHA_OPT_COMMON_H_
#define SCATHA_OPT_COMMON_H_

#include <span>

#include "Common/Base.h"
#include "IR/Fwd.h"

namespace scatha::opt {

SCATHA_TESTAPI
bool preceeds(ir::Instruction const* a, ir::Instruction const* b);

SCATHA_TESTAPI
bool isReachable(ir::Instruction const* from, ir::Instruction const* to);

SCATHA_TESTAPI bool compareEqual(ir::Phi const* lhs, ir::Phi const* rhs);

SCATHA_TESTAPI
bool compareEqual(ir::Phi const* lhs, std::span<ir::ConstPhiMapping const> rhs);

SCATHA_TESTAPI
bool compareEqual(ir::Phi const* lhs, std::span<ir::PhiMapping const> rhs);

/// Replace all uses of \p oldValue with \p newValue
SCATHA_TESTAPI void replaceValue(ir::Value* oldValue, ir::Value* newValue);

/// Fully remove \p *predecessor as predecessor of \p *basicBlock.
/// Caller is responsible for removing \p *basicBlock as successor of \p
/// *predecessor
SCATHA_TESTAPI
void removePredecessorAndUpdatePhiNodes(ir::BasicBlock* basicBlock,
                                        ir::BasicBlock const* predecessor);

/// Remove this value from the operand lists of all its users.
SCATHA_TESTAPI void clearAllUses(ir::Value* value);

/// Split the edge from \p from to \p to by inserting an empty basic block in
/// between
/// \returns The newly created basic block
SCATHA_TESTAPI ir::BasicBlock* splitEdge(ir::Context& ctx,
                                         ir::BasicBlock* from,
                                         ir::BasicBlock* to);

/// Removes critical edges from \p function by inserting empty basic blocks
SCATHA_API bool splitCriticalEdges(ir::Context& ctx, ir::Function& function);

/// \returns `true` iff the instruction \p inst has side effects
SCATHA_TESTAPI bool hasSideEffects(ir::Instruction const* inst);

} // namespace scatha::opt

#endif // SCATHA_OPT_COMMON_H_
