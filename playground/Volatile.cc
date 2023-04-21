#include "Volatile.h"

#include <fstream>
#include <iostream>
#include <sstream>

#include <range/v3/view.hpp>
#include <svm/Program.h>
#include <svm/VirtualMachine.h>
#include <utl/stdio.hpp>

#include "Assembly/Assembler.h"
#include "Assembly/AssemblyStream.h"
#include "Assembly/Print.h"
#include "CodeGen/CodeGen.h"
#include "CodeGen/Passes.h"
#include "Common/Base.h"
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
#include "Opt/DeadFuncElim.h"
#include "Opt/InlineCallsite.h"
#include "Opt/Inliner.h"
#include "Opt/InstCombine.h"
#include "Opt/LoopCanonical.h"
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

static void run(Asm::AssemblyStream const& assembly) {
    auto [program, symbolTable] = Asm::assemble(assembly);
    svm::VirtualMachine vm;
    vm.loadBinary(program.data());
    auto mainPos =
        std::find_if(symbolTable.begin(), symbolTable.end(), [](auto& p) {
            return p.first.starts_with("main");
        });
    if (mainPos == symbolTable.end()) {
        std::cout << "No main function defined!\n";
        return;
    }
    vm.execute(mainPos->second, {});
    using RetType               = uint64_t;
    using SRetType              = uint64_t;
    RetType const retval        = static_cast<RetType>(vm.getRegister(0));
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

static void run(ir::Module const& mod) {
    auto assembly = cg::codegen(mod);
    header(" Assembly ");
    Asm::print(assembly);
    run(assembly);
}

static void run(mir::Module const& mod) {
    auto assembly = cg::lowerToASM(mod);
    header(" Assembly ");
    Asm::print(assembly);
    run(assembly);
}

[[maybe_unused]] static void printIRLiveSets(ir::Function const& F) {
    auto liveSets = ir::LiveSets::compute(F);
    for (auto& bb: F) {
        auto toNames = ranges::views::transform(
            [](ir::Value const* value) { return value->name(); });
        auto* live = liveSets.find(&bb);
        if (!live) {
            continue;
        }
        std::cout << bb.name() << ":\n";
        std::cout << "\tLive in:  " << (live->liveIn | toNames) << "\n";
        std::cout << "\tLive out: " << (live->liveOut | toNames) << "\n";
    }
    std::cout << "\n";
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

    bool const optimize = false;

    if (optimize) {
        if (false) {
            opt::inlineFunctions(ctx, irMod);
            opt::deadFuncElim(ctx, irMod);
        }
        else {
            for (auto& F: irMod) {
                opt::sroa(ctx, F);
                opt::memToReg(ctx, F);
            }
        }
    }
    header(" IR Module ");
    print(irMod);

    header(" MIR Module in SSA form ");
    auto mirMod = cg::lowerToMIR(irMod);
    for (auto& F: mirMod) {
        cg::computeLiveSets(F);
        cg::deadCodeElim(F);
    }
    mir::print(mirMod);

    header(" MIR Module after SSA destruction ");
    for (auto& F: mirMod) {
        cg::destroySSA(F);
    }
    mir::print(mirMod);

    header(" MIR Module after jump elision ");
    for (auto& F: mirMod) {
        cg::allocateRegisters(F);
        cg::elideJumps(F);
    }
    mir::print(mirMod);

    run(mirMod);
}

[[maybe_unused]] static void volPlayground(std::filesystem::path path) {
    auto [ctx, mod] = makeIRModuleFromFile(path);

    auto& f = mod.front();

    opt::sroa(ctx, f);
    opt::memToReg(ctx, f);

    print(mod);

    header(" After Inst Combine ");
    opt::instCombine(ctx, f);
    print(mod);

    header(" After 2. Inst Combine ");
    opt::instCombine(ctx, f);
    print(mod);

    header(" After DCE ");
    opt::dce(ctx, f);
    print(mod);
}

void playground::volatilePlayground(std::filesystem::path path) {
    volPlayground(path);
}
