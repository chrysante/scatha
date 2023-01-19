#ifndef SCATHA_OPT_COMMON_H_
#define SCATHA_OPT_COMMON_H_

#include <span>

#include "Basic/Basic.h"
#include "IR/Common.h"

namespace scatha::opt {

SCATHA(TEST_API) bool preceeds(ir::Instruction const* a, ir::Instruction const* b);

SCATHA(TEST_API) bool isReachable(ir::Instruction const* from, ir::Instruction const* to);

SCATHA(TEST_API) bool compareEqual(ir::Phi const* lhs, ir::Phi const* rhs);

SCATHA(TEST_API) bool compareEqual(ir::Phi const* lhs, std::span<ir::ConstPhiMapping const> rhs);

SCATHA(TEST_API) bool compareEqual(ir::Phi const* lhs, std::span<ir::PhiMapping const> rhs);

SCATHA(TEST_API) bool addressEqual(ir::Value const* lhs, ir::Value const* rhs);

SCATHA(TEST_API) bool isLocalMemory(ir::Value const* address);

SCATHA(TEST_API) void replaceValue(ir::Value* oldValue, ir::Value* newValue);

/// Fully remove \p predecessor as predecessor of \p basicBlock.
/// Caller is responsible for removing \p basicBlock as successor of \p predecessor
SCATHA(TEST_API) void removePredecessorAndUpdatePhiNodes(ir::BasicBlock* basicBlock, ir::BasicBlock const* predecessor);



} // namespace scatha::opt

#endif // SCATHA_OPT_COMMON_H_
