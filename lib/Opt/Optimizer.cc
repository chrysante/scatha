#include "Opt/Optimizer.h"

#include <range/v3/algorithm.hpp>

#include "IR/CFG/Function.h"
#include "IR/Module.h"
#include "Opt/Passes.h"

using namespace scatha;

void opt::optimize(ir::Context& context, ir::Module& mod, int level) {
    switch (level) {
    case 0:
        return;

    case 1:
        opt::inlineFunctions(context, mod);
        opt::deadFuncElim(context, mod);
        opt::forEach(context, mod, opt::splitReturns);
        return;

    default:
        return;
    }
}
