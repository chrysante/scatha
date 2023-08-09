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
#include "Opt/Common.h"
#include "Opt/Optimizer.h"
#include "Opt/PassManager.h"
#include "Opt/Passes.h"
#include "Opt/SCCCallGraph.h"
#include "Parser/Lexer.h"
#include "Parser/Parser.h"
#include "Sema/Analyze.h"
#include "Sema/Print.h"
#include "Sema/SemanticIssue.h"
#include "Sema/SymbolTable.h"
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
    using RetType = uint64_t;
    using SRetType = uint64_t;
    RetType const retval = static_cast<RetType>(vm.getRegister(0));
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
    auto assembly =
        cg::codegen(mod); // , *std::make_unique<cg::DebugLogger>());
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

[[maybe_unused]] static void mirPlayground(std::filesystem::path path) {
    auto [ctx, irMod] = makeIRModuleFromFile(path);
    opt::optimize(ctx, irMod, /* Level = */ 1);
    header("IR Module");
    print(irMod);
    auto mod = cg::codegen(irMod, *std::make_unique<cg::DebugLogger>());
    run(mod);
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
    auto [ctx, mod] = makeIRModuleFromFile(path);
    run(mod);

    header("Optimized");
    opt::optimize(ctx, mod, 1);
    print(mod);
}

[[maybe_unused]] static void frontendPlayground(std::filesystem::path path) {
    std::fstream file(path);
    if (!file) {
        throw;
    }
    std::stringstream sstr;
    sstr << file.rdbuf();
    auto source = sstr.str();

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
    auto analysisResult = sema::analyze(*root, sym, issues);
    if (!issues.empty()) {
        issues.print(source);
    }
    header("AST");
    ast::printTree(*root);
    header("Symbol Table");
    sema::print(sym);

    if (!issues.empty()) {
        return;
    }

    header("IR Module");
    auto [ctx, mod] = ast::lowerToIR(*root, sym, analysisResult);
    ir::print(mod);

    run(mod);

    header("After optimizations");
    opt::optimize(ctx, mod, 1);
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
    //    irPlayground(path);
    for (auto name: opt::PassManager::passes()) {
        std::cout << name << std::endl;
    }
}
