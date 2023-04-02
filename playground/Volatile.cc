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
    auto assembly = cg::codegen(mod);
    Asm::print(assembly);
    std::string const mainName = [&] {
        for (auto& f: mod) {
            if (f.name().starts_with("main")) {
                return std::string(f.name());
            }
        }
        return std::string{};
    }();
    auto program = Asm::assemble(assembly, { std::string(mainName) });
    if (mainName.empty()) {
        std::cout << "No main function found\n";
        return;
    }
    svm::VirtualMachine vm;
    vm.loadProgram(program.data());
    vm.execute();
    uint32_t const retval = static_cast<uint32_t>(vm.getState().registers[0]);
    int32_t const signedRetval = static_cast<int32_t>(retval);
    std::cout << "Program returned: " << retval;
    if (signedRetval < 0) {
        std::cout << " (" << signedRetval << ")";
    }
    std::cout << std::endl << std::endl << std::endl;
}

static std::string readFile(std::filesystem::path path) {
    std::fstream file(path);
    assert(file);
    std::stringstream sstr;
    sstr << file.rdbuf();
    return std::move(sstr).str();
}

auto makeIRModuleFromIR(std::filesystem::path path) {
    auto parseRes = ir::parse(readFile(path));
    if (!parseRes) {
        print(parseRes.error());
        std::exit(-1);
    }
    return std::move(parseRes).value();
}

[[maybe_unused]] static void sroaPlayground(std::filesystem::path path) {
    //    auto [ctx, mod] = makeIRModuleFromFile(path);
    auto [ctx, mod] = makeIRModuleFromIR(path);
    auto phase =
        [ctx = &ctx, mod = &mod](std::string name,
                                 bool (*optFn)(ir::Context&, ir::Function&)) {
        header(" " + name + " ");
        for (auto& function: *mod) {
            optFn(*ctx, function);
        }
        ir::print(*mod);
    };
    phase("sroa", opt::sroa);
    phase("memToReg", opt::memToReg);
    phase("instCombine", opt::instCombine);
    run(mod);
}

[[maybe_unused]] static void inliner(std::filesystem::path path) {
    //    auto [ctx, mod] = makeIRModuleFromFile(path);
    auto [ctx, mod] = makeIRModuleFromIR(path);

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
