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
#include "MIR/LiveInterval.h"
#include "MIR/Module.h"
#include "Opt/Common.h"

using namespace scatha;

TEST_CASE("MIR Liveness", "[codegen][mir]") {
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

TEST_CASE("Program intervals", "[codegen][mir]") {
    using namespace mir;
    SECTION("Compare)") {
        LiveInterval I{ 1, 4 };
        CHECK(compare(I, 0) < 0);
        CHECK(compare(I, 1) == 0);
        CHECK(compare(I, 4) > 0);
        CHECK(compare(I, 5) > 0);
    }
    SECTION("Overlap") {
        CHECK(overlaps(LiveInterval{ 1, 3 }, LiveInterval{ 2, 5 }));
        CHECK(!overlaps(LiveInterval{ 1, 3 }, LiveInterval{ 3, 5 }));
        CHECK(!overlaps(LiveInterval{ 1, 3 }, LiveInterval{ 5, 6 }));
        CHECK(overlaps(LiveInterval{ 0, 10 }, LiveInterval{ 5, 5 }));
        CHECK(!overlaps(LiveInterval{ 0, 10 }, LiveInterval{ 10, 10 }));
    }
    SECTION("Range overlap 1") {
        std::vector<LiveInterval> range = { { 0, 4 }, { 6, 8 }, { 8, 10 } };
        auto overlap = rangeOverlap(range, { 2, 7 });
        REQUIRE(overlap.size() == 2);
        CHECK(overlap[0] == LiveInterval{ 0, 4 });
        CHECK(overlap[1] == LiveInterval{ 6, 8 });
    }
    SECTION("Range overlap 2") {
        std::vector<LiveInterval> range = { { 0, 10 } };
        auto overlap = rangeOverlap(range, { 5, 5 });
        REQUIRE(overlap.size() == 1);
        CHECK(overlap[0] == LiveInterval{ 0, 10 });
    }
    SECTION("Range overlap 3") {
        std::vector<LiveInterval> range = { { 0, 1 }, { 1, 2 }, { 2, 3 },
                                            { 3, 4 }, { 4, 5 }, { 5, 6 } };
        auto overlap = rangeOverlap(range, { 1, 5 });
        REQUIRE(overlap.size() == 4);
        CHECK(overlap[0] == LiveInterval{ 1, 2 });
        CHECK(overlap[1] == LiveInterval{ 2, 3 });
        CHECK(overlap[2] == LiveInterval{ 3, 4 });
        CHECK(overlap[3] == LiveInterval{ 4, 5 });
    }
    SECTION("Range overlap 4") {
        std::vector<LiveInterval> range = {
            { 0, 10 },
        };
        auto overlap = rangeOverlap(range, { 0, 0 });
        REQUIRE(overlap.size() == 0);
    }
    SECTION("Range overlap 5") {
        std::vector<LiveInterval> range = {
            { 0, 10 },
        };
        auto overlap = rangeOverlap(range, { 1, 1 });
        REQUIRE(overlap.size() == 1);
        CHECK(overlap[0] == LiveInterval{ 0, 10 });
    }
}
