#include "Opt/Optimizer.h"

#include <range/v3/algorithm.hpp>

#include "IR/CFG/Function.h"
#include "IR/ForEach.h"
#include "IR/Module.h"
#include "Opt/Passes.h"

using namespace scatha;
using namespace ir;
using namespace opt;

void opt::optimize(Context& context, Module& mod, int level) {
    switch (level) {
    case 0:
        return;

    case 1:
        inlineFunctions(context, mod);
        globalDCE(context, mod);
        forEach(context, mod, opt::splitReturns);
        return;

    default:
        return;
    }
}
