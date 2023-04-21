#include "BasicCompiler.h"

#include <iostream>
#include <stdexcept>

#include <Catch/Catch2.hpp>
#include <range/v3/view.hpp>
#include <svm/Program.h>
#include <svm/VirtualMachine.h>
#include <utl/format.hpp>
#include <utl/functional.hpp>
#include <utl/vector.hpp>

#include "AST/AST.h"
#include "AST/LowerToIR.h"
#include "Assembly/Assembler.h"
#include "Assembly/AssemblyStream.h"
#include "CodeGen/CodeGen.h"
#include "Common/Memory.h"
#include "IR/CFG.h"
#include "IR/Context.h"
#include "IR/Module.h"
#include "IR/Parser.h"
#include "Issue/IssueHandler.h"
#include "MIR/CFG.h"
#include "MIR/Module.h"
#include "Opt/ConstantPropagation.h"
#include "Opt/DCE.h"
#include "Opt/DeadFuncElim.h"
#include "Opt/Inliner.h"
#include "Opt/MemToReg.h"
#include "Parser/Lexer.h"
#include "Parser/Parser.h"
#include "Sema/Analyze.h"
#include "Sema/SemanticIssue.h"

using namespace scatha;

static std::pair<ir::Context, ir::Module> frontEndParse(std::string_view text) {
    issue::LexicalIssueHandler lexIss;
    auto tokens = lex::lex(text, lexIss);
    if (!lexIss.empty()) {
        throw std::runtime_error("Compilation failed");
    }
    issue::SyntaxIssueHandler parseIss;
    auto ast = parse::parse(tokens, parseIss);
    if (!parseIss.empty()) {
        throw std::runtime_error("Compilation failed");
    }
    sema::SymbolTable sym;
    issue::SemaIssueHandler semaIss;
    sema::analyze(*ast, sym, semaIss);
    if (!semaIss.empty()) {
        for (auto& issue: semaIss.issues()) {
            std::cout << " at: " << issue.sourceLocation() << std::endl;
        }
        throw std::runtime_error("Compilation failed");
    }
    ir::Context ctx;
    auto mod = ast::lowerToIR(*ast, sym, ctx);
    return { std::move(ctx), std::move(mod) };
}

static void optimize(ir::Context& ctx, ir::Module& mod) {
    opt::inlineFunctions(ctx, mod);
    opt::deadFuncElim(ctx, mod);
}

static uint64_t run(ir::Module const& mod) {
    auto assembly    = cg::codegen(mod);
    auto [prog, sym] = Asm::assemble(assembly);
    svm::VirtualMachine vm(1024, 1024);
    vm.loadBinary(prog.data());
    auto mainPos = std::find_if(sym.begin(), sym.end(), [](auto& p) {
        return p.first.starts_with("main");
    });
    assert(mainPos != sym.end());
    vm.execute(mainPos->second, {});
    return vm.getRegister(0);
}

static void checkReturnImpl(uint64_t expected,
                            ir::Context& ctx,
                            ir::Module& mod,
                            auto opt) {
    uint64_t unOptResult = run(mod);
    CHECK(unOptResult == expected);
    opt(ctx, mod);
    uint64_t optResult = run(mod);
    CHECK(optResult == expected);
}

void test::checkReturns(u64 value, std::string_view text) {
    auto [ctx, mod] = frontEndParse(text);
    checkReturnImpl(value, ctx, mod, &optimize);
}

void test::checkIRReturns(u64 value, std::string_view text) {
    auto [ctx, mod] = ir::parse(text).value();
    checkReturnImpl(value, ctx, mod, &optimize);
}

void test::checkIRReturns(
    u64 value,
    std::string_view text,
    utl::function_view<void(ir::Context&, ir::Module&)> optFunction) {
    auto [ctx, mod] = ir::parse(text).value();
    checkReturnImpl(value, ctx, mod, optFunction);
}
