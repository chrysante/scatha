#include "CodeGen/Utility.h"

#include "MIR/Instruction.h"

using namespace scatha;
using namespace cg;
using namespace mir;

bool cg::hasSideEffects(Instruction const& inst) {
    return isa<StoreInst>(inst) || isa<CallBase>(inst) ||
           isa<ReturnInst>(inst) || isa<JumpBase>(inst) ||
           isa<CompareInst>(inst) || isa<TestInst>(inst);
}
