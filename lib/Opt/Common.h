#ifndef SCATHA_OPT_COMMON_H_
#define SCATHA_OPT_COMMON_H_

#include <span>
#include <string>

#include "Common/Base.h"
#include "IR/Fwd.h"

namespace scatha::opt {

SCATHA_TESTAPI
bool preceeds(ir::Instruction const* a, ir::Instruction const* b);

SCATHA_TESTAPI
bool isReachable(ir::Instruction const* from, ir::Instruction const* to);

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

/// Split the edge from \p from to \p to by inserting an empty basic block in
/// between
/// \returns The newly created basic block
SCATHA_TESTAPI ir::BasicBlock* splitEdge(std::string name,
                                         ir::Context& ctx,
                                         ir::BasicBlock* from,
                                         ir::BasicBlock* to);

/// \Overload with default name
SCATHA_TESTAPI ir::BasicBlock* splitEdge(ir::Context& ctx,
                                         ir::BasicBlock* from,
                                         ir::BasicBlock* to);

/// Removes critical edges from \p function by inserting empty basic blocks
SCATHA_API bool splitCriticalEdges(ir::Context& ctx, ir::Function& function);

/// Creates a new basic block with name \p name that will be a predecessor of \p
/// BB and a successor of all blocks in \p preds \pre All blocks in \p preds
/// must be predecessors of \p BB \Note This function can be used to create
/// preheaders for loop header with multiple inedges from outside the loop
SCATHA_TESTAPI ir::BasicBlock* addJoiningPredecessor(
    ir::Context& ctx,
    ir::BasicBlock* BB,
    std::span<ir::BasicBlock* const> preds,
    std::string name);

/// \returns `true` iff the instruction \p inst has side effects
SCATHA_TESTAPI bool hasSideEffects(ir::Instruction const* inst);

} // namespace scatha::opt

#endif // SCATHA_OPT_COMMON_H_
