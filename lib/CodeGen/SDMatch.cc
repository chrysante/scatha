#include "CodeGen/SDMatch.h"

#include <iostream>

#include <termfmt/termfmt.h>

#include "IR/Print.h"

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
