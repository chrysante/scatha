#include "Opt/Optimizer.h"

#include <range/v3/algorithm.hpp>

#include "IR/CFG/Function.h"
#include "IR/Module.h"
#include "Opt/DeadFuncElim.h"
#include "Opt/Inliner.h"
#include "Opt/UnifyReturns.h"

using namespace scatha;

void opt::optimize(ir::Context& context, ir::Module& mod, int level) {
    switch (level) {
    case 0:
        return;

    case 1:
        for (auto& function: mod) {
            opt::unifyReturns(context, function);
        }
        opt::inlineFunctions(context, mod);
        opt::deadFuncElim(context, mod);
        for (auto& function: mod) {
            opt::splitReturns(context, function);
        }
        return;

    default:
        return;
    }
}
