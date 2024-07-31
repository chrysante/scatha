#include "IR/ForEach.h"

#include "IR/CFG/Function.h"
#include "IR/Module.h"
#include "IR/PassRegistry.h"

using namespace scatha;
using namespace ir;

SC_REGISTER_MODULE_PASS(ir::forEach, "foreach", PassCategory::Schedule, {});

bool ir::forEach(Context& ctx, Module& mod, FunctionPass const& functionPass,
                 ir::PassArgumentMap const&) {
    bool modified = false;
    for (auto& F: mod) {
        modified |= functionPass(ctx, F, {});
    }
    return modified;
}
