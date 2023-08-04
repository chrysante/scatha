#include "BasicCompiler.h"

#include <iostream>
#include <sstream>
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
#include "IR/CFG.h"
#include "IR/Context.h"
#include "IR/Module.h"
#include "IR/Parser.h"
#include "Issue/IssueHandler.h"
#include "MIR/CFG.h"
#include "MIR/Module.h"
#include "Opt/Optimizer.h"
#include "Parser/Lexer.h"
#include "Parser/Parser.h"
#include "Sema/Analyze.h"
#include "Sema/SemanticIssue.h"
#include "Sema/SymbolTable.h"

using namespace scatha;

[[noreturn]] static void throwIssue(std::string_view source,
                                    Issue const& issue) {
    std::stringstream sstr;
    issue.print(source, sstr);
    throw std::runtime_error(sstr.str());
}

static void validateEmpty(std::string_view source, IssueHandler const& issues) {
    if (!issues.empty()) {
        throwIssue(source, issues.front());
    }
}

static std::pair<ir::Context, ir::Module> frontEndParse(std::string_view text) {
    IssueHandler issues;
    auto ast = parse::parse(text, issues);
    validateEmpty(text, issues);
    sema::SymbolTable sym;
    auto analysisResult = sema::analyze(*ast, sym, issues);
    validateEmpty(text, issues);
    return ast::lowerToIR(*ast, sym, analysisResult);
}

static void optimize(ir::Context& ctx, ir::Module& mod) {
    scatha::opt::optimize(ctx, mod, 1);
}

static uint64_t run(ir::Module const& mod) {
    auto assembly = cg::codegen(mod);
    auto [prog, sym] = Asm::assemble(assembly);
    /// We need 2 megabytes of stack size for the ackermann function test to run
    svm::VirtualMachine vm(1 << 10, 1 << 11);
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

void test::run(std::string_view text) {
    auto [ctx, mod] = frontEndParse(text);
    ::run(mod);
}

void test::checkReturns(u64 value, std::string_view text) {
    auto [ctx, mod] = frontEndParse(text);
    checkReturnImpl(value, ctx, mod, &optimize);
}

void test::checkCompiles(std::string_view text) {
    CHECK_NOTHROW([=] {
        auto [ctx, mod] = frontEndParse(text);
        optimize(ctx, mod);
    }());
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
