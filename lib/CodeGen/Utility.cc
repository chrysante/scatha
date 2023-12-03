#include "CodeGen/Utility.h"

#include "MIR/Instruction.h"

using namespace scatha;
using namespace cg;

bool cg::hasSideEffects(mir::Instruction const* inst) {
    using enum mir::InstCode;
    switch (inst->instcode()) {
    case Store:
    case Call:
    case CallExt:
    case Return:
    case Jump:
    case CondJump:
    case Compare:
    case Test:
        return true;
    default:
        return false;
    }
}
