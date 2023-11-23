#include "Opt/Passes.h"

#include "IR/PassRegistry.h"

using namespace scatha;
using namespace opt;
using namespace ir;

SC_REGISTER_CANONICALIZATION(opt::canonicalize, "canonicalize");

SC_REGISTER_PASS(opt::defaultPass, "default");

bool opt::canonicalize(Context& ctx, Function& function) {
    bool modified = false;
    modified |= unifyReturns(ctx, function);
    modified |= rotateLoops(ctx, function);
    return modified;
}

bool opt::defaultPass(Context& ctx, Function& function) {
    bool modified = false;
    modified |= sroa(ctx, function);
    modified |= instCombine(ctx, function);
    modified |= propagateConstants(ctx, function);
    modified |= dce(ctx, function);
    modified |= simplifyCFG(ctx, function);
    modified |= globalValueNumbering(ctx, function);
    modified |= tailRecElim(ctx, function);
    return modified;
}
