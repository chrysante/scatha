#include "CodeGen/JumpElision.h"

#include "MIR/CFG.h"

using namespace scatha;
using namespace cg;

void cg::elideJumps(mir::Function& F) {
    for (auto& BB: F) {
        auto& term = BB.back();
        if (term.instcode() != mir::InstCode::Jump) {
            continue;
        }
        auto* target = term.operandAt(0);
        if (target == BB.next()) {
            BB.erase(&term);
        }
    }
}
