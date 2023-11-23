#include "Opt/Passes.h"

#include "IR/CFG.h"
#include "IR/PassRegistry.h"
#include "IR/Print.h"

using namespace scatha;

SC_REGISTER_PASS(opt::print, "print");

bool opt::print(ir::Context& ctx, ir::Function& function) {
    ir::print(function);
    return false;
}
