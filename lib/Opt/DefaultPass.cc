#include "Opt/Passes.h"

#include "Opt/PassRegistry.h"

using namespace scatha;
using namespace opt;
using namespace ir;

SC_REGISTER_PASS(opt::defaultPass, "default");

bool opt::defaultPass(Context& ctx, Function& function) {
    bool modified = false;
    modified |= sroa(ctx, function);
    modified |= memToReg(ctx, function);
    modified |= instCombine(ctx, function);
    modified |= propagateConstants(ctx, function);
    modified |= dce(ctx, function);
    modified |= simplifyCFG(ctx, function);
    modified |= tailRecElim(ctx, function);
    return modified;
}
