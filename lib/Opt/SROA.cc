#include "Opt/SROA.h"

#include "IR/Context.h"
#include "IR/CFG.h"

using namespace scatha;
using namespace ir;
using namespace opt;

namespace {

struct SROAContext {
    SROAContext(ir::Context& irCtx, ir::Function& function):
        irCtx(irCtx), function(function) {}
    
    bool run();
    
    ir::Context& irCtx;
    ir::Function& function;
};

} // namespace

bool opt::sroa(ir::Context& irCtx, ir::Function& function) {
    SROAContext ctx(irCtx, function);
    return ctx.run();
}

bool SROAContext::run() {
    for (auto& inst: function.entry()) {
        auto* address = dyncast<Alloca*>(&inst);
        if (!address) {
            continue;
        }
        
    }
    SC_DEBUGFAIL();
}
