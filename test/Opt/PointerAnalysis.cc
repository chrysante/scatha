#include <catch2/catch_test_macros.hpp>

#include <range/v3/algorithm.hpp>

#include "IR/CFG.h"
#include "IR/Context.h"
#include "IR/IRParser.h"
#include "IR/Module.h"
#include "Opt/Passes.h"

using namespace scatha;
using namespace opt;
using namespace ir;

TEST_CASE("Pointer analysis two allocas", "[opt][pointer-analysis]") {
    auto [ctx, mod] = ir::parse(R"(
func i1 @F() {
%entry:
    %a = alloca i64, i32 1
    %b = alloca i64, i32 1
    %eq = ucmp eq ptr %a, ptr %b
    return i1 %eq
})")
                          .value();
    auto& F = mod.front();
    pointerAnalysis(ctx, F);
    instCombine(ctx, F);
    auto& entry = F.entry();
    REQUIRE(ranges::distance(entry) == 1);
    auto* retval = cast<IntegralConstant const*>(
        cast<Return const*>(entry.terminator())->value());
    CHECK(retval->value() == 0);
}

TEST_CASE("Pointer analysis alloca and builtin.alloc",
          "[opt][pointer-analysis]") {
    auto [ctx, mod] = ir::parse(R"(
ext func { ptr, i64 } @__builtin_alloc(i64, i64)

func i1 @F() {
%entry:
    %a = alloca i64, i32 1
    %alloc = call { ptr, i64 } @__builtin_alloc, i64 8, i64 8
    %b = extract_value { ptr, i64 } %alloc, 0
    %eq = ucmp eq ptr %a, ptr %b
    return i1 %eq
})")
                          .value();
    auto& F = mod.front();
    pointerAnalysis(ctx, F);
    instCombine(ctx, F);
    auto& entry = F.entry();
    auto* retval = cast<IntegralConstant const*>(
        cast<Return const*>(entry.terminator())->value());
    CHECK(retval->value() == 0);
}
