#include "Opt/Passes.h"

#include "IR/CFG.h"
#include "IR/Module.h"
#include "IR/PassRegistry.h"

using namespace scatha;
using namespace opt;
using namespace ir;

SC_REGISTER_GLOBAL_PASS(opt::forEach, "foreach");

bool opt::forEach(Context& ctx, Module& mod, LocalPass localPass) {
    bool modified = false;
    for (auto& F: mod) {
        modified |= localPass(ctx, F);
    }
    return modified;
}
