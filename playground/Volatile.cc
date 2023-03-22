#include "Volatile.h"

#include <iostream>
#include <range/v3/view.hpp>
#include <svm/Program.h>
#include <svm/VirtualMachine.h>
#include <utl/stdio.hpp>

#include "Assembly/Assembler.h"
#include "Assembly/AssemblyStream.h"
#include "Assembly/Print.h"
#include "Basic/Basic.h"
#include "CodeGen/IR2ByteCode/CodeGenerator.h"
#include "IR/CFG.h"
#include "IR/Clone.h"
#include "IR/Context.h"
#include "IR/Module.h"
#include "IR/Parser/Parser.h"
#include "IR/Print.h"
#include "IR/Validate.h"
#include "IRDump.h"
#include "Opt/CallGraph.h"
#include "Opt/ConstantPropagation.h"
#include "Opt/InlineCallsite.h"
#include "Opt/Inliner.h"
#include "Opt/MemToReg.h"
#include "Opt/SimplifyCFG.h"

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

using namespace scatha;

void playground::volatilePlayground(std::filesystem::path path) {
    auto [ctx, mod] = makeIRModuleFromFile(path);

    header(" Before inlining ");
    ir::print(mod);

    opt::inlineFunctions(ctx, mod);

    header(" After inlining ");

    ir::print(mod);

    auto assembly = cg::codegen(mod);

    header(" Assembly ");

    Asm::print(assembly);

    auto program = Asm::assemble(assembly);

    svm::VirtualMachine vm;
    vm.loadProgram(program.data());
    vm.execute();
    std::cout << "Program returned: " << vm.getState().registers[0]
              << std::endl;
}
