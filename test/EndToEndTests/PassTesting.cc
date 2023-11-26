#include "test/EndToEndTests/PassTesting.h"

#include <functional>
#include <iostream>
#include <sstream>
#include <stdexcept>

#include <catch2/catch_test_macros.hpp>
#include <range/v3/view.hpp>
#include <svm/Program.h>
#include <svm/VirtualMachine.h>
#include <utl/format.hpp>
#include <utl/functional.hpp>
#include <utl/strcat.hpp>
#include <utl/vector.hpp>

#include "AST/AST.h"
#include "Assembly/Assembler.h"
#include "Assembly/AssemblyStream.h"
#include "CodeGen/CodeGen.h"
#include "IR/Context.h"
#include "IR/ForEach.h"
#include "IR/Fwd.h"
#include "IR/IRParser.h"
#include "IR/Module.h"
#include "IR/PassManager.h"
#include "IR/Pipeline.h"
#include "IRGen/IRGen.h"
#include "Issue/IssueHandler.h"
#include "Opt/Optimizer.h"
#include "Opt/Passes.h"
#include "Parser/Lexer.h"
#include "Parser/Parser.h"
#include "Sema/Analyze.h"
#include "Sema/SymbolTable.h"
#include "test/CoutRerouter.h"
#include "test/Options.h"

using namespace scatha;
using namespace test;

using Generator = utl::unique_function<std::pair<ir::Context, ir::Module>()>;

static void validateEmpty(std::span<SourceFile const> sources,
                          IssueHandler const& issues) {
    if (issues.empty()) {
        return;
    }
    std::stringstream sstr;
    issues.print(sources, sstr);
    throw std::runtime_error(sstr.str());
}

static Generator makeScathaGenerator(std::vector<std::string> sourceTexts) {
    IssueHandler issues;
    auto sourceFiles = sourceTexts | ranges::views::transform([](auto& text) {
                           return SourceFile::make(std::move(text));
                       }) |
                       ranges::to<std::vector>;
    auto ast = parser::parse(sourceFiles, issues);
    validateEmpty(sourceFiles, issues);
    sema::SymbolTable sym;
    auto analysisResult = sema::analyze(*ast, sym, issues);
    validateEmpty(sourceFiles, issues);
    return [ast = std::move(ast),
            sym = std::move(sym),
            analysisResult = std::move(analysisResult)] {
        auto result = irgen::generateIR(*ast, sym, analysisResult, {});
        ir::forEach(result.first, result.second, opt::unifyReturns);
        return result;
    };
}

static Generator makeIRGenerator(std::string_view text) {
    return [=] {
        auto result = ir::parse(text).value();
        ir::forEach(result.first, result.second, opt::unifyReturns);
        return result;
    };
}

static uint64_t run(ir::Module const& mod) {
    auto assembly = cg::codegen(mod);
    auto [prog, sym] = Asm::assemble(assembly);
    /// We need 2 megabytes of stack size for the ackermann function test to run
    svm::VirtualMachine vm(1 << 10, 1 << 12);
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
    ir::Pipeline light =
        ir::PassManager::makePipeline("unifyreturns, sroa, memtoreg");
    ir::Pipeline lightRotate =
        ir::PassManager::makePipeline("canonicalize, sroa, memtoreg");
    ir::Pipeline lightInline =
        ir::PassManager::makePipeline("inline(sroa, memtoreg)");

    static Impl& get() {
        static Impl inst;
        return inst;
    }

    void runTest(Generator const& generator, uint64_t expected) const {
        /// No optimization
        {
            auto [ctx, mod] = generator();
            checkReturns("Unoptimized", mod, expected);
        }

        /// Default optimizations
        {
            auto [ctx, mod] = generator();
            opt::optimize(ctx, mod, 1);
            checkReturns("Default pipeline", mod, expected);
        }

        if (getOptions().TestPasses) {
            for (auto pass: ir::PassManager::localPasses()) {
                ir::Pipeline pipeline(pass);
                testPipeline(generator,
                             ir::PassManager::makePipeline("unifyreturns"),
                             expected,
                             pipeline);
                testPipeline(generator, light, expected, pipeline);
                testPipeline(generator, lightRotate, expected, pipeline);
                testPipeline(generator, lightInline, expected, pipeline);
            }
            auto passes = ir::PassManager::localPasses();
        }

        if (!getOptions().TestPipeline.empty()) {
            auto pipeline =
                ir::PassManager::makePipeline(getOptions().TestPipeline);
            testPipeline(generator,
                         ir::PassManager::makePipeline("unifyreturns"),
                         expected,
                         pipeline);
            testPipeline(generator, light, expected, pipeline);
            testPipeline(generator, lightRotate, expected, pipeline);
            testPipeline(generator, lightInline, expected, pipeline);
        }

        if (getOptions().TestIdempotency) {
            /// Idempotency of passes without prior optimizations
            testIdempotency(generator,
                            ir::PassManager::makePipeline("unifyreturns"),
                            expected);

            /// Idempotency of passes after light optimizations
            testIdempotency(generator, light, expected);
            /// Idempotency of passes after light optimizations and loop
            /// rotation
            testIdempotency(generator, lightRotate, expected);

            /// Idempotency of passes after light inlining optimizations
            testIdempotency(generator, lightInline, expected);
        }
    }

    void checkReturns(std::string_view msg,
                      ir::Module const& mod,
                      uint64_t expected) const {
        INFO(msg);
        CHECK(run(mod) == expected);
    }

    void testPipeline(Generator const& generator,
                      ir::Pipeline const& prePipeline,
                      uint64_t expected,
                      ir::Pipeline const& pipeline) const {
        auto [ctx, mod] = generator();
        prePipeline.execute(ctx, mod);
        auto message = utl::strcat("Pass test for \"",
                                   pipeline,
                                   "\" with pre pipeline \"",
                                   prePipeline,
                                   "\"");
        pipeline(ctx, mod);
        checkReturns(message, mod, expected);
    }

    void testIdempotency(Generator const& generator,
                         ir::Pipeline const& prePipeline,
                         uint64_t expected) const {
        for (auto pass:
             ir::PassManager::localPasses(ir::PassCategory::Simplification))
        {
            if (pass.name() == "default") {
                continue;
            }
            auto [ctx, mod] = generator();
            prePipeline.execute(ctx, mod);
            auto message = utl::strcat("Idempotency check for \"",
                                       pass.name(),
                                       "\" with pre pipeline \"",
                                       prePipeline,
                                       "\"");
            ir::forEach(ctx, mod, pass);
            checkReturns(message, mod, expected);
            bool second = ir::forEach(ctx, mod, pass);
            INFO(message);
            CHECK(!second);
            checkReturns(message, mod, expected);
        }
    }
};

} // namespace

void test::checkReturns(uint64_t expectedResult,
                        std::vector<std::string> sourceTexts) {
    test::CoutRerouter rerouter;
    Impl::get().runTest(makeScathaGenerator(std::move(sourceTexts)),
                        expectedResult);
}

void test::checkIRReturns(uint64_t expectedResult, std::string_view source) {
    test::CoutRerouter rerouter;
    Impl::get().runTest(makeIRGenerator(source), expectedResult);
}

void test::checkCompiles(std::string text) {
    CHECK_NOTHROW([=] {
        auto [ctx, mod] = makeScathaGenerator({ std::move(text) })();
        opt::optimize(ctx, mod, 1);
    }());
}

void test::checkIRCompiles(std::string_view text) {
    CHECK_NOTHROW([=] {
        auto [ctx, mod] = makeIRGenerator(text)();
        opt::optimize(ctx, mod, 1);
    }());
}

void test::compileAndRun(std::string text) {
    auto [ctx, mod] = makeScathaGenerator({ std::move(text) })();
    ::run(mod);
}

void test::compileAndRunIR(std::string text) {
    auto [ctx, mod] = makeIRGenerator({ std::move(text) })();
    ::run(mod);
}

void test::checkPrints(std::string_view printed, std::string source) {
    test::CoutRerouter rerouter;
    compileAndRun(source);
    CHECK(rerouter.str() == printed);
}

void test::checkIRPrints(std::string_view printed, std::string source) {
    test::CoutRerouter rerouter;
    compileAndRunIR(source);
    CHECK(rerouter.str() == printed);
}
