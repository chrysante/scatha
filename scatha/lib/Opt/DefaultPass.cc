#include "Opt/Passes.h"

#include "IR/PassRegistry.h"

using namespace scatha;
using namespace opt;
using namespace ir;

SC_REGISTER_PASS(opt::canonicalize, "canonicalize",
                 PassCategory::Canonicalization, {});

SC_REGISTER_PASS(opt::defaultPass, "default", PassCategory::Simplification, {});

bool opt::canonicalize(Context& ctx, Function& function) {
    bool modified = false;
    modified |= unifyReturns(ctx, function);
    modified |= loopRotate(ctx, function);
    return modified;
}

bool opt::defaultPass(Context& ctx, Function& function) {
    bool modified = false;
    modified |= sroa(ctx, function);
    modified |= pointerAnalysis(ctx, function);
    modified |= instCombine(ctx, function);
    modified |= propagateConstants(ctx, function);
    modified |= dce(ctx, function);
    modified |= simplifyCFG(ctx, function);
    modified |= globalValueNumbering(ctx, function);
    modified |= tailRecElim(ctx, function);
#if 0 // Not working correctly
    modified |= loopUnroll(ctx, function);
#endif
    return modified;
}
