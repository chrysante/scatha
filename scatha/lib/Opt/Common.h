#ifndef SCATHA_OPT_COMMON_H_
#define SCATHA_OPT_COMMON_H_

#include <span>
#include <string>
#include <utility>

#include <svm/Builtin.h>

#include "Common/Base.h"
#include "IR/Fwd.h"

namespace scatha::opt {

/// \Returns `true` if instruction \p a preceeds the instruction \p b
/// \pre Both instructions must be defined in the same basic block
SCTEST_API
bool preceeds(ir::Instruction const* a, ir::Instruction const* b);

/// Moves all Alloca instructions from the basic block \p from into the basic
/// block \p to
/// This expects the allocas to be the first instructions in \p from and inserts
/// them after the set of initial allocas in \p to This is used for inlining
void moveAllocas(ir::BasicBlock* from, ir::BasicBlock* to);

SCTEST_API
bool compareEqual(ir::Phi const* lhs, std::span<ir::ConstPhiMapping const> rhs);

SCTEST_API
bool compareEqual(ir::Phi const* lhs, std::span<ir::PhiMapping const> rhs);

/// Split the edge from \p from to \p to by inserting an empty basic block in
/// between
/// \returns The newly created basic block
SCTEST_API ir::BasicBlock* splitEdge(std::string name, ir::Context& ctx,
                                     ir::BasicBlock* from, ir::BasicBlock* to);

/// \Overload with default name
SCTEST_API ir::BasicBlock* splitEdge(ir::Context& ctx, ir::BasicBlock* from,
                                     ir::BasicBlock* to);

/// Removes critical edges from \p function by inserting empty basic blocks
SCATHA_API bool splitCriticalEdges(ir::Context& ctx, ir::Function& function);

/// Creates a new basic block with name \p name that will be a predecessor of
/// \p BB and a successor of all blocks in \p preds \pre All blocks in \p preds
/// must be predecessors of \p BB
/// \Note This function can be used to create preheaders for loop header with
/// multiple inedges from outside the loop
SCTEST_API ir::BasicBlock* addJoiningPredecessor(
    ir::Context& ctx, ir::BasicBlock* BB,
    std::span<ir::BasicBlock* const> preds, std::string name);

/// \returns `true` if the instruction \p inst has side effects
/// Specifically _side effects_ mean
/// - stores to memory
/// - function calls not marked as 'side effect free'
/// Terminators are not considered to have side effects
SCTEST_API bool hasSideEffects(ir::Instruction const* inst);

/// # Memcpy related queries

/// \Returns `true` if \p inst is a call to memcpy
bool isMemcpy(ir::Instruction const* inst);

/// \Returns `true` if \p inst is a call to memcpy with a constant size argument
bool isConstSizeMemcpy(ir::Instruction const* inst);

/// \Returns the destination pointer of a call to memcpy
ir::Value const* memcpyDest(ir::Instruction const* call);

/// \overload for non-const argument
inline ir::Value* memcpyDest(ir::Instruction* call) {
    return const_cast<ir::Value*>(memcpyDest(&std::as_const(*call)));
}

/// \Returns the source pointer of the memcpy operation
ir::Value const* memcpySource(ir::Instruction const* call);

/// \overload for non-const argument
inline ir::Value* memcpySource(ir::Instruction* call) {
    return const_cast<ir::Value*>(memcpySource(&std::as_const(*call)));
}

/// \Returns the size of the memcpy operation
size_t memcpySize(ir::Instruction const* call);

/// Sets the destination pointer of the call to memcpy \p call to \p dest
void setMemcpyDest(ir::Instruction* call, ir::Value* dest);

/// Sets the source pointer of the call to memcpy \p call to \p source
void setMemcpySource(ir::Instruction* call, ir::Value* source);

/// # Memset related queries

/// \Returns `true` if \p inst is a call to memset
bool isMemset(ir::Instruction const* inst);

/// \Returns `true` if \p inst is a call to memset with a constant size and
/// constant value argument
bool isConstMemset(ir::Instruction const* inst);

/// \Returns `true` if \p inst is a call to memset with a constant size and
/// constant value argument equal to zero
bool isConstZeroMemset(ir::Instruction const* inst);

/// \Returns the destination pointer of a call to memcpy
ir::Value const* memsetDest(ir::Instruction const* call);

/// \overload for non-const argument
inline ir::Value* memsetDest(ir::Instruction* call) {
    return const_cast<ir::Value*>(memsetDest(&std::as_const(*call)));
}

/// Sets the destination pointer of the call to memset \p call to \p dest
void setMemsetDest(ir::Instruction* call, ir::Value* dest);

/// \Returns the size of the memset operation
size_t memsetSize(ir::Instruction const* call);

/// \Returns the size of the memset operation
ir::Value const* memsetValue(ir::Instruction const* call);

/// \overload for non-const argument
inline ir::Value* memsetValue(ir::Instruction* call) {
    return const_cast<ir::Value*>(memsetValue(&std::as_const(*call)));
}

/// \Returns the size of the memset operation as constant
int64_t memsetConstValue(ir::Instruction const* call);

/// # builtin function related queries

/// \Returns `true` if \p value is a call to the builtin function \p builtin
bool isBuiltinCall(ir::Value const* value, svm::Builtin builtin);

} // namespace scatha::opt

#endif // SCATHA_OPT_COMMON_H_
