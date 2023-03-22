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
    for (auto& f: mod.functions()) {
        opt::memToReg(ctx, f);
        opt::propagateConstants(ctx, f);
    }
    header(" Before inlining ");
    ir::print(mod);
    opt::inlineFunctions(ctx, mod);
    ir::assertInvariants(ctx, mod);

    for (auto& f: mod.functions()) {
        opt::memToReg(ctx, f);
        opt::propagateConstants(ctx, f);
        opt::simplifyCFG(ctx, f);
    }

    ir::assertInvariants(ctx, mod);

    header(" After inlining ");
    ir::print(mod);

    //    auto& main      = mod.functions().back();
    //    auto& f         = mod.functions().front();
    //    assert(main.name().starts_with("main"));
    //    /// Optimize a bit to get more readable output
    //    opt::memToReg(ctx, main);
    //    opt::memToReg(ctx, f);
    //    header(" Before inlining ");
    //    ir::print(mod);
    //    auto itr =
    //        ranges::find_if(main.instructions(), [](ir::Instruction const&
    //        inst) {
    //            return isa<ir::FunctionCall>(inst);
    //        });
    //    if (itr == ranges::end(main.instructions())) {
    //        std::cout << "No call instruction in main to inline\n";
    //        return;
    //    }
    //    auto& callInst = *itr;
    //    opt::inlineCallsite(ctx, cast<ir::FunctionCall*>(&callInst));
    //    header(" After inlining ");
    //    ir::print(mod);
    //    auto const assembly = cg::codegen(mod);
    //    auto const program =
    //        Asm::assemble(assembly, { .startFunction =
    //        std::string(main.name()) });
    //    svm::VirtualMachine vm;
    //    vm.loadProgram(program.data());
    //    vm.execute();
    //    u64 const exitCode = vm.getState().registers[0];
    //    std::cout << "VM: Program ended with exit code: [\n\ti: "
    //              << static_cast<i64>(exitCode) << ", \n\tu: " << exitCode
    //              << ", \n\tf: " << utl::bit_cast<f64>(exitCode) << "\n]"
    //              << std::endl;
}
