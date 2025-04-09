#include "CodeGen/SDMatch.h"

#include "IR/CFG/Instructions.h"

using namespace scatha;
using namespace cg;

bool MatcherBase::match(ir::Instruction const& inst,
                        SelectionNode& node) const {
    for (auto& matchCase: matchCases) {
        if (matchCase(inst, node)) {
            return true;
        }
    }
    return false;
}

bool MatcherBase::canDeferLoad(ir::Load const* load,
                               ir::Value const* value) const {
    auto* inst = dyncast<ir::Instruction const*>(value);
    if (!inst) return true;
    return !DAG().dependencies(DAG(inst)).contains(DAG(load));
}
