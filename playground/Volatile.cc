#include "Volatile.h"

#include <fstream>
#include <iostream>
#include <range/v3/view.hpp>
#include <sstream>
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
#include "IR/Parser.h"
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
#include "Opt/TailRecElim.h"
#include "Opt/UnifyReturns.h"

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

static void run(ir::Module const& mod) {
    auto assembly              = cg::codegen(mod);
    std::string const mainName = [&] {
        for (auto& f: mod.functions()) {
            if (f.name().starts_with("main")) {
                return std::string(f.name());
            }
        }
        return std::string{};
    }();
    if (mainName.empty()) {
        std::cout << "No main function found\n";
        return;
    }
    auto program = Asm::assemble(assembly, { std::string(mainName) });
    svm::VirtualMachine vm;
    vm.loadProgram(program.data());
    vm.execute();
    std::cout << "Program returned: " << vm.getState().registers[0] << std::endl
              << std::endl
              << std::endl;
}

static std::string readFile(std::filesystem::path path) {
    std::fstream file(path);
    assert(file);
    std::stringstream sstr;
    sstr << file.rdbuf();
    return std::move(sstr).str();
}

[[maybe_unused]] static void sroaPlayground(std::filesystem::path path) {
    //    auto [ctx, mod] = makeIRModuleFromFile(path);

    auto parseRes = ir::parse(readFile(path));
    if (!parseRes) {
        SC_DEBUGFAIL();
    }
    auto [ctx, mod] = std::move(parseRes).value();

    auto clone = ir::clone(ctx, &mod.functions().front());

    ir::print(*clone);

    return;

    auto phase =
        [ctx = &ctx, mod = &mod](std::string name,
                                 bool (*optFn)(ir::Context&, ir::Function&)) {
        header(" " + name + " ");
        for (auto& function: mod->functions()) {
            optFn(*ctx, function);
        }
        ir::print(*mod);
    };
    run(mod);
    phase("SROA", opt::sroa);
    phase("M2R", opt::memToReg);
    phase("Inst Combine", opt::instCombine);
    phase("DCE", opt::dce);
    phase("SCCP", opt::propagateConstants);
    phase("SCFG", opt::simplifyCFG);
    phase("TRE", opt::tailRecElim);
    run(mod);
}

[[maybe_unused]] static void inliner(std::filesystem::path path) {
    //    auto [ctx, mod] = makeIRModuleFromFile(path);

    auto parseRes = ir::parse(readFile(path));
    if (!parseRes) {
        print(parseRes.error());
        return;
    }
    auto [ctx, mod] = std::move(parseRes).value();

    header(" Before inlining ");
    ir::print(mod);

    run(mod);

    header(" After inlining ");
    opt::inlineFunctions(ctx, mod);
    ir::print(mod);

    run(mod);
}

void playground::volatilePlayground(std::filesystem::path path) {
    inliner(path);
}
