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
#include "IR/Context.h"
#include "IR/Module.h"
#include "IR/Parser/Parser.h"
#include "IR/Print.h"
#include "IRDump.h"
#include "Opt/CallGraph.h"
#include "Opt/InlineCallsite.h"
#include "Opt/MemToReg.h"

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
    auto& main      = mod.functions().front();
    auto& f         = mod.functions().back();

    /// Optimize a bit to get more readable output
    opt::memToReg(ctx, main);
    opt::memToReg(ctx, f);

    header(" Before inlining ");
    ir::print(mod);

    auto& callInst =
        *ranges::find_if(main.instructions(), [](ir::Instruction const& inst) {
            return isa<ir::FunctionCall>(inst);
        });
    opt::inlineCallsite(ctx, cast<ir::FunctionCall*>(&callInst));

    header(" After inlining ");
    ir::print(mod);

    auto const assembly = cg::codegen(mod);
    auto const program =
        Asm::assemble(assembly, { .startFunction = std::string(main.name()) });

    svm::VirtualMachine vm;
    vm.loadProgram(program.data());
    vm.execute();
    u64 const exitCode = vm.getState().registers[0];
    std::cout << "VM: Program ended with exit code: [\n\ti: "
              << static_cast<i64>(exitCode) << ", \n\tu: " << exitCode
              << ", \n\tf: " << utl::bit_cast<f64>(exitCode) << "\n]"
              << std::endl;
}
