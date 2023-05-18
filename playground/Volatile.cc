#include "Volatile.h"

#include <fstream>
#include <iostream>
#include <sstream>

#include <range/v3/view.hpp>
#include <svm/Program.h>
#include <svm/VirtualMachine.h>
#include <utl/stdio.hpp>

#include "AST/LowerToIR.h"
#include "AST/Print.h"
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
#include "IR/Type.h"
#include "IR/Validate.h"
#include "IRDump.h"
#include "Issue/IssueHandler.h"
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
#include "Parser/Lexer.h"
#include "Parser/Parser.h"
#include "Sema/Analyze.h"
#include "Sema/SemanticIssue.h"
#include "Util.h"

using namespace scatha;
using namespace playground;

static void run(Asm::AssemblyStream const& assembly) {
    auto [program, symbolTable] = Asm::assemble(assembly);
    // svm::print(program.data());
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
    header("Assembly");
    Asm::print(assembly);
    header("Execution");
    run(assembly);
}

static void run(mir::Module const& mod) {
    auto assembly = cg::lowerToASM(mod);
    header("Assembly");
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

    header("Before inlining");
    ir::print(mod);
    run(mod);

    header("After inlining");
    opt::inlineFunctions(ctx, mod);
    ir::print(mod);

    run(mod);
}

[[maybe_unused]] static void mirPlayground(std::filesystem::path path) {
    auto [ctx, irMod] = makeIRModuleFromFile(path);

    bool const optimize = true;

    if (optimize) {
        opt::inlineFunctions(ctx, irMod);
        // opt::deadFuncElim(ctx, irMod);
    }
    header("IR Module");
    print(irMod);

    header("MIR Module in SSA form");
    auto mirMod = cg::lowerToMIR(irMod);
    for (auto& F: mirMod) {
        cg::computeLiveSets(F);
        cg::deadCodeElim(F);
    }
    mir::print(mirMod);

    header("MIR Module after SSA destruction");
    for (auto& F: mirMod) {
        cg::destroySSA(F);
    }
    mir::print(mirMod);

    header("MIR Module after reg alloc");
    for (auto& F: mirMod) {
        cg::allocateRegisters(F);
    }
    mir::print(mirMod);

    run(mirMod);
}

static void pass(std::string_view name,
                 ir::Context& ctx,
                 ir::Module& mod,
                 bool (*optFn)(ir::Context&, ir::Function&)) {
    header("After " + std::string(name));
    for (auto& F: mod) {
        optFn(ctx, F);
    }
    ir::print(mod);
}

[[maybe_unused]] static void irPlayground(std::filesystem::path path) {
    header("Parsed program");
    auto [ctx, mod] = makeIRModuleFromFile(path);

    pass("M2R", ctx, mod, opt::memToReg);
    pass("TRE", ctx, mod, opt::tailRecElim);
    pass("TRE", ctx, mod, opt::tailRecElim);

    run(mod);
}

[[maybe_unused]] static void frontendPlayground(std::filesystem::path path) {
    std::fstream file(path);
    if (!file) {
        throw;
    }
    std::stringstream sstr;
    sstr << file.rdbuf();
    auto source = sstr.str();

    header("AST");
    IssueHandler issues;
    auto root = parse::parse(source, issues);
    if (!issues.empty()) {
        issues.print(source);
    }
    if (!root) {
        return;
    }
    issues.clear();
    sema::SymbolTable sym;
    sema::analyze(*root, sym, issues);
    if (!issues.empty()) {
        issues.print(source);
    }
    ast::printTree(*root);
    if (!issues.empty()) {
        return;
    }

    header("IR Module");
    ir::Context ctx;
    auto mod = ast::lowerToIR(*root, sym, ctx);
    ir::print(mod);

    run(mod);

    header("After Inliner");
    opt::inlineFunctions(ctx, mod);
    opt::deadFuncElim(ctx, mod);
    ir::print(mod);

    run(mod);
}

[[maybe_unused]] static void lexPlayground(std::filesystem::path path) {
    std::fstream file(path);
    if (!file) {
        return;
    }
    std::stringstream sstr;
    sstr << file.rdbuf();
    auto text = sstr.str();
    IssueHandler iss;
    auto tokens = parse::lex(text, iss);
    for (auto& tok: tokens) {
        std::cout << tok.id() << std::endl;
    }
}

void playground::volatilePlayground(std::filesystem::path path) {
    inliner(path);
}
