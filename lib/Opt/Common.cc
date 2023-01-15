#include "Opt/Common.h"

#include <utl/hashset.hpp>

#include "IR/CFG.h"

using namespace scatha;
using namespace opt;
using namespace ir;

bool opt::preceeds(Instruction const* a, Instruction const* b) {
    SC_ASSERT(a->parent() == b->parent(), "a and b must be in the same basic block");
    auto* bb              = a->parent();
    auto const* const end = bb->instructions.end().to_address();
    for (; a != end; a = a->next()) {
        if (a == b) {
            return true;
        }
    }
    return false;
}

bool opt::isReachable(Instruction const* from, Instruction const* to) {
    SC_ASSERT(from != to, "from and to are equal. Does that mean they are reachable or not?");
    SC_ASSERT(from->parent()->parent() == to->parent()->parent(),
              "The instructions must be in the same function for this to be sensible");
    if (from->parent() == to->parent()) {
        /// From and to are in the same basic block. If \p from preceeds \p to then \p to is definitely reachable.
        if (preceeds(from, to)) {
            return true;
        }
    }
    /// If they are not in the same basic block or \p to comes before \p from perform a DFS to check if we can reach the
    /// BB of \p to from the BB of \p from.
    auto search = [&, target = to->parent()](BasicBlock const* bb, auto& search) -> bool {
        if (bb == target) {
            return true;
        }
        for (BasicBlock const* succ: bb->successors()) {
            if (search(succ, search)) {
                return true;
            }
        }
        return false;
    };
    return search(from->parent(), search);
}

static bool cmpEqImpl(ir::Phi const* lhs, auto rhs) {
    if (lhs->argumentCount() != rhs.size()) {
        return false;
    }
    auto lhsArgs = lhs->arguments();
    utl::hashset<ConstPhiMapping> lhsSet(lhsArgs.begin(), lhsArgs.end());
    utl::hashset<ConstPhiMapping> rhsSet(rhs.begin(), rhs.end());
    return lhsSet == rhsSet;
}

bool opt::compareEqual(ir::Phi const* lhs, std::span<ir::ConstPhiMapping const> rhs) {
    return cmpEqImpl(lhs, rhs);
}

bool opt::compareEqual(ir::Phi const* lhs, std::span<ir::PhiMapping const> rhs) {
    return cmpEqImpl(lhs, rhs);
}

bool opt::compareEqual(ir::Phi const* lhs, ir::Phi const* rhs) {
    SC_ASSERT(lhs->parent()->parent() == rhs->parent()->parent(),
              "The phi nodes must be in the same function for this comparison to be sensible");
    return cmpEqImpl(lhs, rhs->arguments());
}

void opt::replaceValue(Value* oldValue, Value* newValue) {
    /// We need this funny way of traversing the user list of the old value, because if the loop body the user is erased
    /// from the user list and iterators are invalidated.
    while (!oldValue->users().empty()) {
        auto* user = *oldValue->users().begin();
        for (auto [index, op]: utl::enumerate(user->operands())) {
            if (op == oldValue) {
                user->setOperand(index, newValue);
            }
        }
    }
}
