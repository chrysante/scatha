#ifndef SCATHA_OPT_COMMON_H_
#define SCATHA_OPT_COMMON_H_

#include <span>
#include <string>
#include <utility>

#include "Common/Base.h"
#include "IR/Fwd.h"

namespace scatha::opt {

SCATHA_TESTAPI
bool preceeds(ir::Instruction const* a, ir::Instruction const* b);

SCATHA_TESTAPI
bool isReachable(ir::Instruction const* from, ir::Instruction const* to);

/// Moves all Alloca instructions from the basic block \p from into the basic
/// block \p to
/// This expects the allocas to be the first instructions in \p from and inserts
/// them after the set of initial allocas in \p to This is used for inlining
void moveAllocas(ir::BasicBlock* from, ir::BasicBlock* to);

SCATHA_TESTAPI
bool compareEqual(ir::Phi const* lhs, std::span<ir::ConstPhiMapping const> rhs);

SCATHA_TESTAPI
bool compareEqual(ir::Phi const* lhs, std::span<ir::PhiMapping const> rhs);

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

/// Creates a new basic block with name \p name that will be a predecessor of
/// \p BB and a successor of all blocks in \p preds \pre All blocks in \p preds
/// must be predecessors of \p BB
/// \Note This function can be used to create preheaders for loop header with
/// multiple inedges from outside the loop
SCATHA_TESTAPI ir::BasicBlock* addJoiningPredecessor(
    ir::Context& ctx,
    ir::BasicBlock* BB,
    std::span<ir::BasicBlock* const> preds,
    std::string name);

/// \returns `true` if the instruction \p inst has side effects
SCATHA_TESTAPI bool hasSideEffects(ir::Instruction const* inst);

/// \Returns `true` if \p callInst is a call instruction that calls the builtin
/// function at index \p functionIndex
SCATHA_TESTAPI bool isBuiltinCall(ir::Instruction const* callInst,
                                  size_t functionIndex);

/// # Memcpy related queries

/// \Returns `true` if \p call is a call to memcpy
bool isMemcpy(ir::Call const* call);

/// \Returns `true` if \p call is a call to memcpy with a constant size argument
bool isConstSizeMemcpy(ir::Call const* call);

/// \Returns the destination pointer of a call to memcpy
ir::Value const* memcpyDest(ir::Call const* call);

/// \overload for non-const argument
inline ir::Value* memcpyDest(ir::Call* call) {
    return const_cast<ir::Value*>(memcpyDest(&std::as_const(*call)));
}

/// \Returns the source pointer of the memcpy operation
ir::Value const* memcpySource(ir::Call const* call);

/// \overload for non-const argument
inline ir::Value* memcpySource(ir::Call* call) {
    return const_cast<ir::Value*>(memcpySource(&std::as_const(*call)));
}

/// \Returns the destination pointer of the memcpy operation
size_t memcpySize(ir::Call const* call);

/// Sets the destination pointer of the call to memcpy \p call to \p dest
void setMemcpyDest(ir::Call* call, ir::Value* dest);

/// Sets the destination pointer of the call to memcpy \p call to \p source
void setMemcpySource(ir::Call* call, ir::Value* source);

} // namespace scatha::opt

#endif // SCATHA_OPT_COMMON_H_
