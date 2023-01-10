#include "OptTest.h"

#include <utl/stdio.hpp>

#include "IR/Module.h"
#include "IR/Print.h"
#include "Opt/Mem2Reg.h"

#include "IRDump.h"

using namespace playground;

static int const headerWidth = 60;

static void line(std::string_view m) {
    utl::print("{:=^{}}\n", m, headerWidth);
};

static void header(std::string_view title = "") {
    utl::print("\n");
    line("");
    line(title);
    line("");
    utl::print("\n");
}

void playground::optTest(std::filesystem::path filepath) {
    scatha::ir::Module mod = makeIRModule(filepath);
    scatha::opt::mem2Reg(mod);
    header(" Before mem2reg ");
    scatha::ir::print(mod);
    scatha::opt::mem2Reg(mod);
    header(" After mem2reg ");
    scatha::ir::print(mod);
}
