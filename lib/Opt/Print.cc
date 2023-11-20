#include "Opt/Passes.h"

#include "IR/CFG.h"
#include "IR/Print.h"
#include "Opt/PassRegistry.h"

using namespace scatha;

SC_REGISTER_PASS(opt::print, "print");

bool opt::print(ir::Context& ctx, ir::Function& function) {
    ir::print(function);
    return false;
}
