#include "IR/ForEach.h"

#include "IR/CFG.h"
#include "IR/Module.h"
#include "IR/PassRegistry.h"

using namespace scatha;
using namespace ir;

SC_REGISTER_GLOBAL_PASS(ir::forEach, "foreach", PassCategory::Other);

bool ir::forEach(Context& ctx, Module& mod, LocalPass localPass) {
    bool modified = false;
    for (auto& F: mod) {
        modified |= localPass(ctx, F);
    }
    return modified;
}
