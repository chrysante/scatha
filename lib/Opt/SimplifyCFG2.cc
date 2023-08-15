/// ↑
///  ```
///       │
///       ↓
///     ┌───┐
///     │---│
///     │---│
///     └───┘
///       │
///       ↓
///     ┌───┐
///     │---│
///     │---│
///     └───┘
///       │
///       ↓
///  ```
///  ```
///       │
///       ↓
///     ┌───┐
///     │---│
///     │---│
///     │---│
///     │---│
///     └───┘
///       │
///       ↓
///  ```

#include "Opt/Passes.h"

#include "IR/CFG.h"
#include "IR/Context.h"
#include "Opt/PassRegistry.h"

using namespace scatha;
using namespace ir;
using namespace opt;

#if 0
SC_REGISTER_PASS(opt::simplifyCFG2, "simplifycfg2");
#endif

namespace {

struct SCFGContext {
    Context& ctx;
    Function& function;

    SCFGContext(Context& ctx, Function& function):
        ctx(ctx), function(function) {}

    bool run();
};

} // namespace

bool opt::simplifyCFG2(ir::Context& ctx, ir::Function& function) {
    SC_UNIMPLEMENTED();
}

bool SCFGContext::run() { SC_UNIMPLEMENTED(); }
