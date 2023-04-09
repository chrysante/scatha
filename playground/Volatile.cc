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
#include "CodeGen/IRToMIR.h"
#include "IR/CFG.h"
#include "IR/Clone.h"
#include "IR/Context.h"
#include "IR/DataFlow.h"
#include "IR/Dominance.h"
#include "IR/Loop.h"
#include "IR/Module.h"
#include "IR/Parser.h"
#include "IR/Print.h"
#include "IR/Validate.h"
#include "IRDump.h"
#include "MIR/CFG.h"
#include "MIR/Module.h"
#include "MIR/Print.h"
#include "Opt/ConstantPropagation.h"
#include "Opt/DCE.h"
#include "Opt/InlineCallsite.h"
#include "Opt/Inliner.h"
#include "Opt/InstCombine.h"
#include "Opt/MemToReg.h"
#include "Opt/SCCCallGraph.h"
#include "Opt/SROA.h"
#include "Opt/SimplifyCFG.h"
#include "Opt/TailRecElim.h"
#include "Opt/UnifyReturns.h"

static void line(std::string_view m) {
    std::cout << "==============================" << m
              << "==============================\n";
};

static void header(std::string_view title = "") {
    std::cout << "\n";
    line("");
    line(title);
    line("");
    std::cout << "\n";
}

using namespace scatha;
using namespace playground;

static void run(ir::Module const& mod) {
    auto assembly = cg::codegen(mod);
    header(" Assembly ");
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
    using RetType        = uint64_t;
    using SRetType       = uint64_t;
    RetType const retval = static_cast<RetType>(vm.getState().registers[0]);
    SRetType const signedRetval = static_cast<SRetType>(retval);
    std::cout << "Program returned: " << retval;
    std::cout << "\n                 (" << std::hex << retval << std::dec
              << ")";
    if (signedRetval < 0) {
        std::cout << "\n                 (" << signedRetval << ")";
    }
    std::cout << "\n                 (" << utl::bit_cast<double>(retval) << ")";
    std::cout << std::endl << std::endl << std::endl;
}

[[maybe_unused]] static void volPlayground(std::filesystem::path path) {
    auto [ctx, mod] = makeIRModuleFromFile(path);

    auto& f = mod.front();
    opt::memToReg(ctx, f);

    print(mod);

    auto liveSets = ir::LiveSets::compute(f);
    for (auto& bb: f) {
        auto toNames = ranges::views::transform(
            [](ir::Value const* value) { return value->name(); });
        auto& live = liveSets.find(&bb);
        std::cout << bb.name() << ":\n";
        std::cout << "\tLive in:  " << (live.liveIn | toNames) << "\n";
        std::cout << "\tLive out: " << (live.liveOut | toNames) << "\n";
    }
}

[[maybe_unused]] static void inliner(std::filesystem::path path) {
    auto [ctx, mod] = makeIRModuleFromFile(path);

    header(" Before inlining ");
    ir::print(mod);
    run(mod);

    header(" After inlining ");
    opt::inlineFunctions(ctx, mod);
    ir::print(mod);

    run(mod);
}

[[maybe_unused]] static void mirPG(std::filesystem::path path) {
    auto [ctx, irMod] = makeIRModuleFromFile(path);

    //    print(irMod);

    //    auto& f       = irMod.front();
    //    auto liveSets = ir::LiveSets::compute(f);
    //    for (auto& bb: f) {
    //        auto toNames = ranges::views::transform(
    //            [](ir::Value const* value) { return value->name(); });
    //        auto& live = liveSets.find(&bb);
    //        std::cout << bb.name() << ":\n";
    //        std::cout << "\tLive in:  " << (live.liveIn | toNames) << "\n";
    //        std::cout << "\tLive out: " << (live.liveOut | toNames) << "\n";
    //    }

    {
        header(" no opt ");
        print(irMod);
        auto mirMod = cg::lowerToMIR(irMod);
        mir::print(mirMod);
    }
    return;
    {
        opt::inlineFunctions(ctx, irMod);
        header(" opt ");
        print(irMod);
        auto mirMod = cg::lowerToMIR(irMod);
        mir::print(mirMod);
    }
}

void playground::volatilePlayground(std::filesystem::path path) { mirPG(path); }
