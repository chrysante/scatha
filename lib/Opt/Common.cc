#include "Opt/Common.h"

#include "IR/CFG.h"

using namespace scatha;
using namespace opt;
using namespace ir;

bool opt::preceeds(Instruction const* a, Instruction const* b) {
    SC_ASSERT(a->parent() == b->parent(), "a and b must be in the same basic block");
    auto* bb = a->parent();
    auto const* const end = bb->instructions.end().to_address();
    for (; a != end; a = a->next()) {
        if (a == b) {
            return true;
        }
    }
    return false;
}

void opt::replaceValue(Value* oldValue, Value* newValue) {
    for (auto* user: oldValue->users()) {
        for (auto [index, op]: utl::enumerate(user->operands())) {
            if (op == oldValue) {
                user->setOperand(index, newValue);
            }
        }
    }
}
