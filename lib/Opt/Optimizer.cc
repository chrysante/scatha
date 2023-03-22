#include "Opt/Optimizer.h"

#include "Opt/Inliner.h"

using namespace scatha;

void opt::optimize(ir::Context& context, ir::Module& mod, int level) {
    switch (level) {
    case 0:
        return;

    case 1:
        opt::inlineFunctions(context, mod);
        return;

    default:
        return;
    }
}
