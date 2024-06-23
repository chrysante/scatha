#include "EndToEndTests/PassTesting.h"

#include <functional>
#include <iostream>
#include <sstream>
#include <stdexcept>

#include <catch2/catch_test_macros.hpp>
#include <range/v3/algorithm.hpp>
#include <range/v3/view.hpp>
#include <svm/Program.h>
#include <svm/VirtualMachine.h>
#include <termfmt/termfmt.h>
#include <utl/functional.hpp>
#include <utl/strcat.hpp>
#include <utl/vector.hpp>

#include "AST/AST.h"
#include "Assembly/Assembler.h"
#include "Assembly/AssemblyStream.h"
#include "CodeGen/CodeGen.h"
#include "CodeGen/Logger.h"
#include "Common/FFI.h"
#include "IR/Context.h"
#include "IR/ForEach.h"
#include "IR/Fwd.h"
#include "IR/IRParser.h"
#include "IR/Module.h"
#include "IR/PassManager.h"
#include "IR/Pipeline.h"
#include "IR/Print.h"
#include "IRGen/IRGen.h"
#include "Issue/IssueHandler.h"
#include "Main/Options.h"
#include "Opt/Passes.h"
#include "Parser/Lexer.h"
#include "Parser/Parser.h"
#include "Sema/Analyze.h"
#include "Sema/SymbolTable.h"
#include "Util/CoutRerouter.h"

using namespace scatha;
using namespace test;

using Generator = utl::unique_function<
    std::tuple<ir::Context, ir::Module, std::vector<ForeignLibraryDecl>>()>;

static void validateEmpty(std::span<SourceFile const> sources,
                          IssueHandler const& issues) {
    if (issues.haveErrors()) {
        std::stringstream sstr;
        issues.print(sources, sstr);
        throw std::runtime_error(sstr.str());
    }
}

static Generator makeScathaGenerator(std::vector<std::string> sourceTexts) {
    IssueHandler issues;
    auto sourceFiles = sourceTexts | ranges::views::transform([](auto& text) {
        return SourceFile::make(std::move(text));
    }) | ranges::to<std::vector>;
    auto ast = parser::parse(sourceFiles, issues);
    validateEmpty(sourceFiles, issues);
    sema::SymbolTable sym;
    auto analysisResult = sema::analyze(*ast, sym, issues);
    validateEmpty(sourceFiles, issues);
    return [ast = std::move(ast), sym = std::move(sym),
            analysisResult = std::move(analysisResult)] {
        ir::Context ctx;
        ir::Module mod;
        irgen::generateIR(ctx, mod, *ast, sym, analysisResult, {});
        ir::forEach(ctx, mod, opt::unifyReturns);
        return std::tuple{ std::move(ctx), std::move(mod),
                           sym.foreignLibraries() };
    };
}

static Generator makeIRGenerator(std::string_view text) {
    return [=] {
        auto result = ir::parse(text).value();
        ir::forEach(result.first, result.second, opt::unifyReturns);
        return std::tuple{ std::move(result).first, std::move(result).second,
                           std::vector<ForeignLibraryDecl>{} };
    };
}

static auto codegenAndAssemble(
    ir::Module const& mod, std::ostream* str = nullptr,
    std::span<ForeignLibraryDecl const> foreignLibs = {}) {
    auto assembly = [&] {
        if (!str) {
            return cg::codegen(mod);
        }
        cg::DebugLogger logger(*str);
        return cg::codegen(mod, logger);
    }();
    auto [prog, sym, unresolved] = Asm::assemble(assembly);
    if (!Asm::link(Asm::LinkerOptions{}, prog, foreignLibs, unresolved)) {
        throw std::runtime_error("Linker error");
    }
    return std::pair{ std::move(prog), std::move(sym) };
}

static uint64_t run(ir::Module const& mod, std::ostream* str,
                    std::span<ForeignLibraryDecl const> foreignLibs) {
    auto [prog, sym] = codegenAndAssemble(mod, str, foreignLibs);
    return runProgram(prog, findMain(sym).value());
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
        runTest(generator, [] {},
                [&](u64 retval) { CHECK(retval == expected); });
    }

    void runTest(Generator const& generator, utl::function_view<void()> begin,
                 utl::function_view<void(u64)> end) const {
        /// No optimization
        {
            auto [ctx, mod, libs] = generator();
            runChecked("Unoptimized", mod, libs, begin, end);
        }

        /// Default optimizations
        {
            auto [ctx, mod, libs] = generator();
            opt::optimize(ctx, mod);
            runChecked("Default pipeline", mod, libs, begin, end);
        }

        if (getOptions().TestPasses) {
            for (auto pass: ir::PassManager::localPasses()) {
                if (pass.category() == ir::PassCategory::Experimental) {
                    continue;
                }
                ir::Pipeline pipeline(pass);
                testPipeline(generator,
                             ir::PassManager::makePipeline("unifyreturns"),
                             pipeline, begin, end);
                testPipeline(generator, light, pipeline, begin, end);
                testPipeline(generator, lightRotate, pipeline, begin, end);
                testPipeline(generator, lightInline, pipeline, begin, end);
            }
            auto passes = ir::PassManager::localPasses();
        }

        if (!getOptions().TestPipeline.empty()) {
            auto pipeline =
                ir::PassManager::makePipeline(getOptions().TestPipeline);
            testPipeline(generator,
                         ir::PassManager::makePipeline("unifyreturns"),
                         pipeline, begin, end);
            testPipeline(generator, light, pipeline, begin, end);
            testPipeline(generator, lightRotate, pipeline, begin, end);
            testPipeline(generator, lightInline, pipeline, begin, end);
        }

        if (getOptions().TestIdempotency) {
            /// Idempotency of passes without prior optimizations
            testIdempotency(generator,
                            ir::PassManager::makePipeline("unifyreturns"),
                            begin, end);

            /// Idempotency of passes after light optimizations
            testIdempotency(generator, light, begin, end);
            /// Idempotency of passes after light optimizations and loop
            /// rotation
            testIdempotency(generator, lightRotate, begin, end);

            /// Idempotency of passes after light inlining optimizations
            testIdempotency(generator, lightInline, begin, end);
        }
    }

    void runChecked(std::string_view msg, ir::Module const& mod,
                    std::span<ForeignLibraryDecl const> foreignLibs,
                    utl::function_view<void()> begin,
                    utl::function_view<void(u64)> end) const {
        INFO(msg);
        begin();
        size_t result = 0;
        std::string code;
        if (!getOptions().PrintCodegen) {
            result = run(mod, nullptr, foreignLibs);
        }
        else {
            std::stringstream sstr;
            /// Catch2 breaks strings after 75 characters
            tfmt::setWidth(sstr, 75);
            ir::print(mod, sstr);
            result = run(mod, &sstr, foreignLibs);
            code = std::move(sstr).str();
        }
        INFO(code);
        end(result);
    }

    void testPipeline(Generator const& generator,
                      ir::Pipeline const& prePipeline,
                      ir::Pipeline const& pipeline,
                      utl::function_view<void()> begin,
                      utl::function_view<void(u64)> end) const {
        auto [ctx, mod, libs] = generator();
        prePipeline(ctx, mod);
        auto message = utl::strcat("Pass test for \"", pipeline,
                                   "\" with pre pipeline \"", prePipeline,
                                   "\"");
        pipeline(ctx, mod);
        runChecked(message, mod, libs, begin, end);
    }

    void testIdempotency(Generator const& generator,
                         ir::Pipeline const& prePipeline,
                         utl::function_view<void()> begin,
                         utl::function_view<void(u64)> end) const {
        for (auto pass:
             ir::PassManager::localPasses(ir::PassCategory::Simplification))
        {
            if (pass.name() == "default") {
                continue;
            }
            if (pass.name() == "optimize") {
                continue;
            }
            auto [ctx, mod, libs] = generator();
            prePipeline.execute(ctx, mod);
            auto message = utl::strcat("Idempotency check for \"", pass.name(),
                                       "\" with pre pipeline \"", prePipeline,
                                       "\"");
            ir::forEach(ctx, mod, pass);
            runChecked(message, mod, libs, begin, end);
            bool second = ir::forEach(ctx, mod, pass);
            INFO(message);
            CHECK(!second);
            runChecked(message, mod, libs, begin, end);
        }
    }
};

} // namespace

void test::runReturnsTest(uint64_t expectedResult,
                          std::vector<std::string> sourceTexts) {
    test::CoutRerouter rerouter;
    Impl::get().runTest(makeScathaGenerator(std::move(sourceTexts)),
                        expectedResult);
}

void test::runIRReturnsTest(uint64_t expectedResult, std::string_view source) {
    test::CoutRerouter rerouter;
    Impl::get().runTest(makeIRGenerator(source), expectedResult);
}

void test::runPrintsTest(std::string_view expected,
                         std::vector<std::string> sourceTexts) {
    test::CoutRerouter rerouter;
    auto begin = [&] { rerouter.reset(); };
    auto end = [&](u64) { CHECK(rerouter.str() == expected); };
    Impl::get().runTest(makeScathaGenerator(std::move(sourceTexts)), begin,
                        end);
}

void test::runIRPrintsTest(std::string_view expected, std::string source) {
    test::CoutRerouter rerouter;
    auto begin = [&] { rerouter.reset(); };
    auto end = [&](u64) { CHECK(rerouter.str() == expected); };
    Impl::get().runTest(makeIRGenerator(source), begin, end);
}

bool test::compiles(std::string text) {
    try {
        auto [ctx, mod, libs] = makeScathaGenerator({ std::move(text) })();
        opt::optimize(ctx, mod);
        return true;
    }
    catch (...) {
        return false;
    }
}

bool test::IRCompiles(std::string_view text) {
    try {
        auto [ctx, mod, libs] = makeIRGenerator(text)();
        opt::optimize(ctx, mod);
        return true;
    }
    catch (...) {
        return false;
    }
}

void test::compile(std::string text) {
    auto [ctx, mod, libs] = makeScathaGenerator({ std::move(text) })();
    codegenAndAssemble(mod);
}

uint64_t test::runProgram(std::span<uint8_t const> program, size_t startpos) {
    /// We need 2 megabytes of stack size for the ackermann function test to run
    svm::VirtualMachine vm(1 << 10, 1 << 12);
    vm.loadBinary(program.data());
    vm.execute(startpos, {});
    return vm.getRegister(0);
}

std::optional<size_t> test::findMain(
    std::unordered_map<std::string, size_t> const& sym) {
    auto itr = ranges::find_if(sym, [](auto& p) {
        return p.first.starts_with("main");
    });
    if (itr != sym.end()) {
        return itr->second;
    }
    return std::nullopt;
}

uint64_t test::compileAndRun(std::string text) {
    auto [ctx, mod, libs] = makeScathaGenerator({ std::move(text) })();
    return ::run(mod, nullptr, libs);
}

uint64_t test::compileAndRunIR(std::string text) {
    auto [ctx, mod, libs] = makeIRGenerator({ std::move(text) })();
    return ::run(mod, nullptr, libs);
}

void test::checkReturns(uint64_t expected, std::string source) {
    uint64_t result = compileAndRun(source);
    CHECK(result == expected);
}

void test::checkIRReturns(uint64_t expected, std::string source) {
    uint64_t result = compileAndRunIR(source);
    CHECK(result == expected);
}

void test::checkPrints(std::string_view expected, std::string source) {
    test::CoutRerouter rerouter;
    compileAndRun(source);
    CHECK(rerouter.str() == expected);
}
