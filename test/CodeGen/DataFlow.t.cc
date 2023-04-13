#include <Catch/Catch2.hpp>

#include <range/v3/algorithm.hpp>
#include <string>

#include "CodeGen/DataFlow.h"
#include "CodeGen/LowerToMIR.h"
#include "IR/Context.h"
#include "IR/Module.h"
#include "IR/Parser.h"
#include "MIR/CFG.h"
#include "MIR/Module.h"
#include "Opt/Common.h"

using namespace scatha;

TEST_CASE("MIR Liveness", "[codegen][MIR]") {
    auto const text   = R"(
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
    auto [ctx, irMod] = ir::parse(text).value();
    auto mod          = cg::lowerToMIR(irMod);
    auto& F           = mod.front();
    cg::computeLiveSets(F);
    auto* entry  = F.entry();
    auto* argReg = F.ssaArgumentRegisters().front();
    auto* nReg   = entry->front().dest();
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
