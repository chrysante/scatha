#include "test/Opt/PassTest.h"

#include <Catch/Catch2.hpp>

#include "IR/CFG.h"
#include "IR/Context.h"
#include "IR/Module.h"
#include "IR/Parser.h"
#include "test/IR/Equal.h"

using namespace scatha;
using namespace test;

void test::passTest(opt::LocalPass pass,
                    ir::Context& fCtx,
                    ir::Function& F,
                    ir::Function& ref) {
    bool const modifiedFirstTime = pass(fCtx, F);
    CHECK(modifiedFirstTime);
    bool const modifiedSecondTime = pass(fCtx, F);
    CHECK(!modifiedSecondTime);
    CHECK(test::funcEqual(F, ref));
}

void test::passTest(opt::LocalPass pass,
                    std::string fSource,
                    std::string refSource) {
    auto [fCtx, fMod] = ir::parse(fSource).value();
    auto [refCtx, refMod] = ir::parse(refSource).value();
    auto& F = fMod.front();
    auto& ref = refMod.front();
    test::passTest(pass, fCtx, F, ref);
}

void test::passTest(opt::GlobalPass pass,
                    opt::LocalPass local,
                    ir::Context& mCtx,
                    ir::Module& M,
                    ir::Module& ref) {
    bool const modifiedFirstTime = pass(mCtx, M, local);
    CHECK(modifiedFirstTime);
    bool const modifiedSecondTime = pass(mCtx, M, local);
    CHECK(!modifiedSecondTime);
    for (auto&& [F, G]: ranges::views::zip(M, ref)) {
        CHECK(test::funcEqual(F, G));
    }
}

void test::passTest(opt::GlobalPass pass,
                    opt::LocalPass local,
                    std::string mSource,
                    std::string refSource) {
    auto [mCtx, M] = ir::parse(mSource).value();
    auto [refCtx, ref] = ir::parse(refSource).value();
    test::passTest(pass, local, mCtx, M, ref);
}
