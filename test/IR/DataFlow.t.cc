#include <Catch/Catch2.hpp>

#include <range/v3/algorithm.hpp>
#include <string>

#include "IR/CFG.h"
#include "IR/Context.h"
#include "IR/DataFlow.h"
#include "IR/Module.h"
#include "IR/Parser.h"
#include "Opt/Common.h"

using namespace scatha;

TEST_CASE("Liveness", "[ir][opt]") {
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

    auto [ctx, mod] = ir::parse(text).value();
    auto& F = mod.front();

    auto liveSets = ir::LiveSets::compute(F);

    auto* entry = &F.front();
    auto* param = &F.parameters().front();
    auto* n = &entry->front();
    auto* entryLS = liveSets.find(entry);
    REQUIRE(entryLS);
    CHECK(entryLS->liveIn.contains(param));
    CHECK(entryLS->liveOut.contains(n));

    auto* thenBlock = entry->next();
    auto* thenLS = liveSets.find(thenBlock);
    REQUIRE(thenLS);
    CHECK(thenLS->liveIn.contains(n));
    CHECK(thenLS->liveOut.contains(n));

    auto* elseBlock = thenBlock->next();
    auto* elseLS = liveSets.find(elseBlock);
    REQUIRE(elseLS);
    CHECK(elseLS->liveIn.contains(n));
    CHECK(elseLS->liveOut.contains(n));

    auto* end = elseBlock->next();
    auto* endLS = liveSets.find(end);
    REQUIRE(endLS);
    CHECK(endLS->liveIn.contains(n));
}
