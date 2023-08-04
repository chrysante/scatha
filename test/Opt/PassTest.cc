#include "test/Opt/PassTest.h"

#include <Catch/Catch2.hpp>

#include "IR/CFG.h"
#include "IR/Context.h"
#include "IR/Module.h"
#include "IR/Parser.h"
#include "test/IR/Equal.h"

using namespace scatha;
using namespace test;

void test::passTest(std::function<bool(ir::Context&, ir::Function&)> pass,
                    ir::Context& fCtx,
                    ir::Function& F,
                    ir::Function& ref) {
    bool const modifiedFirstTime = pass(fCtx, F);
    CHECK(modifiedFirstTime);
    bool const modifiedSecondTime = pass(fCtx, F);
    CHECK(!modifiedSecondTime);
    CHECK(test::funcEqual(F, ref));
}

void test::passTest(std::function<bool(ir::Context&, ir::Function&)> pass,
                    std::string fSource,
                    std::string refSource) {
    auto [fCtx, fMod]     = ir::parse(fSource).value();
    auto [refCtx, refMod] = ir::parse(refSource).value();
    auto& F               = fMod.front();
    auto& ref             = refMod.front();
    test::passTest(pass, fCtx, F, ref);
}
