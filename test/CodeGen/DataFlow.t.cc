#include <catch2/catch_test_macros.hpp>

#include <range/v3/algorithm.hpp>
#include <string>

#include "CodeGen/Passes.h"
#include "IR/CFG.h"
#include "IR/Context.h"
#include "IR/IRParser.h"
#include "IR/Module.h"
#include "MIR/CFG.h"
#include "MIR/Context.h"
#include "MIR/Module.h"
#include "Opt/Common.h"

using namespace scatha;

TEST_CASE("MIR Liveness", "[codegen][MIR]") {
    auto const text = R"(
func i64 @f(i64 %0) {
  %entry:
    %n = add i64 %0, i64 1
    %cmp.result = scmp eq i64 %0, i64 0
    branch i1 %cmp.result, label %then, label %else
  
  %then:
    goto label %end

  %else:
    goto label %end
  
  %end:
    %m = add i64 %n, i64 1
    return i64 %m
})";
    auto [irCtx, irMod] = ir::parse(text).value();
    mir::Context ctx;
    auto mod = cg::lowerToMIR(ctx, irMod);
    auto& F = mod.front();
    cg::computeLiveSets(ctx, F);
    auto* entry = F.entry();
    auto* argReg = F.ssaArgumentRegisters().front();
    auto* nReg = entry->front().dest();
    CHECK(entry->isLiveIn(argReg));
    CHECK(entry->isLiveOut(nReg));
    auto* thenBlock = entry->next();
    CHECK(thenBlock->isLiveIn(nReg));
    CHECK(thenBlock->isLiveOut(nReg));
    auto* elseBlock = thenBlock->next();
    CHECK(elseBlock->isLiveIn(nReg));
    CHECK(elseBlock->isLiveOut(nReg));
    auto* end = elseBlock->next();
    CHECK(end->isLiveIn(nReg));
}
