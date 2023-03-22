#include "Opt/DCE.h"

#include <utl/hashtable.hpp>

#include "IR/CFG.h"
#include "IR/Context.h"
#include "IR/Validate.h"
#include "Opt/Common.h"

using namespace scatha;
using namespace opt;
using namespace ir;

namespace {

struct DCEContext {
    explicit DCEContext(Context& irCtx, Function& function):
        irCtx(irCtx), function(function) {}

    bool run();

    Context& irCtx;
    Function& function;
    bool modified = false;
};

} // namespace

bool opt::dce(ir::Context& context, ir::Function& function) {
    DCEContext ctx(context, function);
    bool const result = ctx.run();
    ir::assertInvariants(context, function);
    return result;
}

bool DCEContext::run() {
    return modified;
}
