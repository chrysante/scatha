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
#include "IR/Dominance.h"
#include "IR/Module.h"
#include "IR/Parser/Parser.h"
#include "IR/Print.h"
#include "IR/Validate.h"
#include "IRDump.h"
#include "Opt/CallGraph.h"
#include "Opt/ConstantPropagation.h"
#include "Opt/DCE.h"
#include "Opt/InlineCallsite.h"
#include "Opt/Inliner.h"
#include "Opt/InstCombine.h"
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
    header(" After SROA ");
    for (auto& function: mod.functions()) {
        opt::sroa(ctx, function);
    }
    ir::print(mod);
    header(" After M2R ");
    for (auto& function: mod.functions()) {
        opt::memToReg(ctx, function);
    }
    ir::print(mod);
    header(" After InstCombine ");
    for (auto& function: mod.functions()) {
        opt::instCombine(ctx, function);
    }
    ir::print(mod);
    header(" After DCE ");
    for (auto& function: mod.functions()) {
        opt::dce(ctx, function);
    }
    ir::print(mod);
    header(" After SCCP ");
    for (auto& function: mod.functions()) {
        opt::propagateConstants(ctx, function);
    }
    ir::print(mod);
    header(" After SCFG ");
    for (auto& function: mod.functions()) {
        opt::simplifyCFG(ctx, function);
    }
    ir::print(mod);

    header(" Generated assembly ");
    auto assembly = cg::codegen(mod);
    Asm::print(assembly);

    auto& main   = mod.functions().front();
    auto program = Asm::assemble(assembly, { std::string(main.name()) });

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

    std::cout << "Program returned: " << vm.getState().registers[0] << std::endl
              << std::endl
              << std::endl;
}

[[maybe_unused]] static void inlinerAndSimplifyCFG(std::filesystem::path path) {
    auto [ctx, mod] = makeIRModuleFromFile(path);

    header(" Before inlining ");
    ir::print(mod);

    header(" After inlining ");
    opt::inlineFunctions(ctx, mod);
    ir::print(mod);

    //    header(" Assembly ");
    auto assembly = cg::codegen(mod);
    //    Asm::print(assembly);

    auto& main   = mod.functions().front();
    auto program = Asm::assemble(assembly, { std::string(main.name()) });

    svm::VirtualMachine vm;
    vm.loadProgram(program.data());
    vm.execute();

    std::cout << "Program returned: " << vm.getState().registers[0]
              << std::endl;
}

void playground::volatilePlayground(std::filesystem::path path) {
    sroaPlayground(path);
}
