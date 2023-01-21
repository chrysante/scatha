#include "Opt/Optimizer.h"

#include "IR/CFG.h"
#include "IR/Context.h"
#include "IR/Module.h"
#include "Opt/ConstantPropagation.h"
#include "Opt/DCE.h"
#include "Opt/Mem2Reg.h"

using namespace scatha;
using namespace opt;

void opt::optimize(ir::Context& context, ir::Module& mod, int level) {
    switch (level) {
    case 0:
        return;
        
    case 1:
        for (auto& function: mod.functions()) {
            opt::mem2Reg(context, function);
            opt::propagateConstants(context, function);
            opt::dce(context, function);
        }
        return;
        
    default:
        return;
    }
}
