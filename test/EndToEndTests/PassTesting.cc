#include "test/EndToEndTests/PassTesting.h"

#include <functional>
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
#include "IR/Context.h"
#include "IR/Fwd.h"
#include "IR/Module.h"
#include "IR/Parser.h"
#include "Issue/IssueHandler.h"
#include "Opt/Optimizer.h"
#include "Opt/PassManager.h"
#include "Opt/Passes.h"
#include "Opt/Pipeline.h"
#include "Parser/Lexer.h"
#include "Parser/Parser.h"
#include "Sema/Analyze.h"
#include "Sema/SemanticIssue.h"
#include "Sema/SymbolTable.h"
#include "test/Options.h"

using namespace scatha;
using namespace test;

using ParserType =
    std::function<std::pair<ir::Context, ir::Module>(std::string_view)>;

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

static std::pair<ir::Context, ir::Module> parseScatha(std::string_view text) {
    IssueHandler issues;
    auto ast = parse::parse(text, issues);
    validateEmpty(text, issues);
    sema::SymbolTable sym;
    auto analysisResult = sema::analyze(*ast, sym, issues);
    validateEmpty(text, issues);
    auto result = ast::lowerToIR(*ast, sym, analysisResult);
    opt::forEach(result.first, result.second, opt::unifyReturns);
    return result;
}

static std::pair<ir::Context, ir::Module> parseIR(std::string_view text) {
    auto result = ir::parse(text).value();
    opt::forEach(result.first, result.second, opt::unifyReturns);
    return result;
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

using ParserType =
    std::function<std::pair<ir::Context, ir::Module>(std::string_view)>;

namespace {

struct Impl {
    // sroa:memtoreg
    opt::Pipeline light;

    // inline(sroa:memtoreg)
    opt::Pipeline lightInline;

    Impl() {
        light = opt::PassManager::makePipeline("sroa, memtoreg");
        lightInline = opt::PassManager::makePipeline("inline(sroa, memtoreg)");
    }

    static Impl& get() {
        static Impl inst;
        return inst;
    }

    void runTest(std::string_view source,
                 uint64_t expected,
                 ParserType parse) const {
        /// No optimization
        {
            auto [ctx, mod] = parse(source);
            checkReturns("Unoptimized", mod, expected);
        }

        /// Default optimizations
        {
            auto [ctx, mod] = parse(source);
            opt::optimize(ctx, mod, 1);
            checkReturns("Default pipeline", mod, expected);
        }

        if (getOptions().TestIdempotency) {
            /// Idempotency of passes without prior optimizations
            testIdempotency(source, parse, opt::Pipeline());

            /// Idempotency of passes after light optimizations
            testIdempotency(source, parse, light);

            /// Idempotency of passes after light inlining optimizations
            testIdempotency(source, parse, lightInline);
        }
    }

    void checkReturns(std::string_view msg,
                      ir::Module const& mod,
                      uint64_t expected) const {
        INFO(msg);
        CHECK(run(mod) == expected);
    }

    void testIdempotency(std::string_view source,
                         ParserType parse,
                         opt::Pipeline const& prePipeline) const {
        for (auto pass: opt::PassManager::localPasses()) {
            auto [ctx, mod] = parse(source);
            prePipeline.execute(ctx, mod);
            if (pass.name() == "unifyreturns") {
                opt::forEach(ctx, mod, opt::splitReturns);
            }
            testIdempotency(ctx, mod, pass);
        }
    }

    void testIdempotency(ir::Context& ctx,
                         ir::Module& mod,
                         opt::LocalPass pass) const {
        opt::forEach(ctx, mod, pass);
        bool second = opt::forEach(ctx, mod, pass);
        INFO("Idempotency check for \"" << pass.name() << "\"");
        CHECK(!second);
    }
};

} // namespace

void test::checkReturns(uint64_t expectedResult, std::string_view source) {
    Impl::get().runTest(source, expectedResult, parseScatha);
}

void test::checkIRReturns(uint64_t expectedResult, std::string_view source) {
    Impl::get().runTest(source, expectedResult, parseIR);
}

void test::checkCompiles(std::string_view text) {
    CHECK_NOTHROW([=] {
        auto [ctx, mod] = parseScatha(text);
        opt::optimize(ctx, mod, 1);
    }());
}

void test::compileAndRun(std::string_view text) {
    auto [ctx, mod] = parseScatha(text);
    ::run(mod);
}
