#include "Opt/Passes.h"

#include <range/v3/algorithm.hpp>

#include "IR/CFG.h"
#include "IR/ForEach.h"
#include "IR/Module.h"
#include "IR/PassRegistry.h"
#include "Opt/Passes.h"

using namespace scatha;
using namespace ir;
using namespace opt;

SC_REGISTER_GLOBAL_PASS(opt::optimize, "optimize", PassCategory::Optimization,
                        {});

bool opt::optimize(Context& ctx, Module& mod, LocalPass) {
    bool modified = false;
    modified |= inlineFunctions(ctx, mod);
    modified |= globalDCE(ctx, mod);
    modified |= forEach(ctx, mod, splitReturns);
    return modified;
}
