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
#include "Opt/SROA.h"
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
using namespace playground;

[[maybe_unused]] static void sroaPlayground(std::filesystem::path path) {
    auto [ctx, mod] = makeIRModuleFromFile(path);
    header(" Before SROA ");
    ir::print(mod);
    for (auto& function: mod.functions()) {
        opt::sroa(ctx, function);
    }
    header(" After SROA ");
    ir::print(mod);
    for (auto& function: mod.functions()) {
        opt::memToReg(ctx, function);
    }
    header(" After M2R ");
    ir::print(mod);
}

[[maybe_unused]] static void inlinerAndSimplifyCFG(std::filesystem::path path) {
    auto [ctx, mod] = makeIRModuleFromFile(path);

    header(" Before inlining ");
    ir::print(mod);

    opt::inlineFunctions(ctx, mod);

    header(" After inlining ");

    ir::print(mod);

    auto assembly = cg::codegen(mod);

    header(" Assembly ");

    Asm::print(assembly);

    auto& main = mod.functions().front();

    auto program = Asm::assemble(assembly, { std::string(main.name()) });

    header(" Program ");

    svm::print(program.data());

    svm::VirtualMachine vm;
    vm.loadProgram(program.data());
    vm.execute();

    std::cout << "Registers: \n";
    for (auto [index, value]: vm.getState().registers |
                                  ranges::views::take(10) |
                                  ranges::views::enumerate)
    {
        std::cout << "[" << index << "] = " << value << std::endl;
    }

    std::cout << "Program returned: " << vm.getState().registers[0]
              << std::endl;
}

void playground::volatilePlayground(std::filesystem::path path) {
    sroaPlayground(path);
}
