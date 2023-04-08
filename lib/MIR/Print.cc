#include "MIR/Print.h"

#include <iostream>

#include "MIR/CFG.h"
#include "MIR/Module.h"

using namespace scatha;
using namespace mir;

void mir::print(mir::Module const& mod) { mir::print(mod, std::cout); }

void mir::print(mir::Module const& mod, std::ostream& ostream) {
    for (auto& F: mod) {
        print(F, ostream);
    }
}

void mir::print(mir::Function const& F) { mir::print(F, std::cout); }

void mir::print(mir::Function const& F, std::ostream& ostream) {}
