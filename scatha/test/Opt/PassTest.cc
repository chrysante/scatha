#include "Opt/PassTest.h"

#include <catch2/catch_test_macros.hpp>

#include "IR/CFG.h"
#include "IR/Context.h"
#include "IR/Equal.h"
#include "IR/IRParser.h"
#include "IR/Module.h"
#include "IR/PipelineParser.h"

using namespace scatha;
using namespace test;

void test::passTest(ir::FunctionPass pass, ir::Context& fCtx, ir::Function& F,
                    ir::Function& ref) {
    bool const modifiedFirstTime = pass(fCtx, F);
    CHECK(modifiedFirstTime);
    bool const modifiedSecondTime = pass(fCtx, F);
    CHECK(!modifiedSecondTime);
    CHECK(test::funcEqual(F, ref));
}

void test::passTest(ir::FunctionPass pass, std::string fSource,
                    std::string refSource) {
    auto [fCtx, fMod] = ir::parse(fSource).value();
    auto [refCtx, refMod] = ir::parse(refSource).value();
    auto& F = fMod.front();
    auto& ref = refMod.front();
    test::passTest(pass, fCtx, F, ref);
}

void test::passTest(ir::Pipeline const& pipeline, ir::Context& mCtx,
                    ir::Module& M, ir::Module& ref) {
    bool const modifiedFirstTime = pipeline(mCtx, M);
    CHECK(modifiedFirstTime);
    bool const modifiedSecondTime = pipeline(mCtx, M);
    CHECK(!modifiedSecondTime);
    for (auto&& [F, G]: ranges::views::zip(M, ref)) {
        CHECK(test::funcEqual(F, G));
    }
}

void test::passTest(ir::Pipeline const& pipeline, std::string mSource,
                    std::string refSource) {
    auto [mCtx, M] = ir::parse(mSource).value();
    auto [refCtx, ref] = ir::parse(refSource).value();
    test::passTest(pipeline, mCtx, M, ref);
}

void test::passTest(ir::ModulePass pass, ir::FunctionPass local,
                    std::string mSource, std::string refSource) {
    return passTest(ir::Pipeline(std::move(pass), std::move(local)),
                    std::move(mSource), std::move(refSource));
}

void test::passTest(std::string pipeline, std::string mSource,
                    std::string refSource) {
    return passTest(ir::parsePipeline(pipeline), std::move(mSource),
                    std::move(refSource));
}
