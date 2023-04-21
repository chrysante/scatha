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

} // namespace scatha::opt

#endif // SCATHA_OPT_COMMON_H_
