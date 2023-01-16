#include "Opt/SCC.h"

#include "IR/CFG.h"
#include "IR/Context.h"
#include "IR/Module.h"

using namespace scatha;
using namespace opt;
using namespace ir;

namespace {

/// One context object is created per analyzed function.
struct SCCContext {
    explicit SCCContext(Context& irCtx, Function& function): irCtx(irCtx), function(function) {}

    void run();

    Context& irCtx;
    Function& function;
};

} // namespace

void opt::scc(ir::Context& context, ir::Module& mod) {
    for (auto& function: mod.functions()) {
        SCCContext ctx(context, function);
        ctx.run();
    }
}

void SCCContext::run() {}
