#include <catch2/catch_test_macros.hpp>

#include <range/v3/algorithm.hpp>

#include "IR/CFG.h"
#include "IR/Context.h"
#include "IR/IRParser.h"
#include "IR/Module.h"
#include "Opt/PassTest.h"
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

TEST_CASE("Pointer analysis alloca and __builtin_alloc",
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

TEST_CASE("Compare pointers in same allocation", "[opt][pointer-analysis]") {
    test::passTest("ptranalysis, instcombine, sroa, instcombine",
                   R"(
@array = constant [i64, 2] [i64 0, i64 1]
  #ptr(align: 8, validsize: 16, provenance: ptr @array, offset: 0, nonnull)

ext func void @__builtin_memcpy(ptr %0, i64 %1, ptr %2, i64 %3)

func i1 @test() {
  %entry:
    %i.addr = alloca i64, i32 2
    call void @__builtin_memcpy, ptr %i.addr, i64 16, ptr @array, i64 16
    %elem.addr = getelementptr inbounds i64, ptr %i.addr, i64 0
    %elem.addr.0 = getelementptr inbounds i64, ptr %i.addr, i64 1
    %eq = ucmp eq ptr %elem.addr, ptr %elem.addr.0
    return i1 %eq
})",
                   R"(
func i1 @test() {
  %entry:
    return i1 0
})");
}

TEST_CASE("Compare unknown pointer to non-escaping alloca",
          "[opt][pointer-analysis]") {
    test::passTest("ptranalysis, instcombine, sroa",
                   R"(
func i1 @main() {
  %entry:
    %i.addr = alloca i64, i32 1
    store ptr %i.addr, i64 0
    %call.result = call ptr @makePtr
    %eq = ucmp eq ptr %i.addr, ptr %call.result
    return i1 %eq
}
func ptr @makePtr() {
  %entry:
    return ptr nullptr
})",
                   R"(
func i1 @main() {
  %entry:
    %call.result = call ptr @makePtr
    return i1 0
}
func ptr @makePtr() {
  %entry:
    return ptr nullptr
})");
}

TEST_CASE("Compare unknown pointer to non-escaping dynamic allocation",
          "[opt][pointer-analysis]") {
    test::passTest("ptranalysis, instcombine, simplifycfg",
                   R"(
ext func { ptr, i64 } @__builtin_alloc(i64 %0, i64 %1)
ext func void @__builtin_dealloc(ptr %0, i64 %1, i64 %2)
func i1 @main() {
  %entry:
    %unique.alloc = call { ptr, i64 } @__builtin_alloc, i64 8, i64 8
    %unique.pointer = extract_value { ptr, i64 } %unique.alloc, 0
    %call.result = call ptr @makePtr
    %eq = ucmp eq ptr %unique.pointer, ptr %call.result
    %unique.ptr.engaged = ucmp neq ptr %unique.pointer, ptr nullptr
    branch i1 %unique.ptr.engaged, label %unique.ptr.delete, label %unique.ptr.end

  %unique.ptr.delete: // preds: entry
    call void @__builtin_dealloc, ptr %unique.pointer, i64 8, i64 8
    goto label %unique.ptr.end

  %unique.ptr.end: // preds: entry, unique.ptr.delete
    return i1 %eq
}
func ptr @makePtr() {
  %entry:
    return ptr nullptr
})",
                   R"(
ext func { ptr, i64 } @__builtin_alloc(i64 %0, i64 %1)
ext func void @__builtin_dealloc(ptr %0, i64 %1, i64 %2)
func i1 @main() {
  %entry:
    %unique.alloc = call { ptr, i64 } @__builtin_alloc, i64 8, i64 8
    %unique.pointer = extract_value { ptr, i64 } %unique.alloc, 0
    %call.result = call ptr @makePtr
    call void @__builtin_dealloc, ptr %unique.pointer, i64 8, i64 8
    return i1 0
}
func ptr @makePtr() {
  %entry:
    return ptr nullptr
})");
}

TEST_CASE("Compare alloca to pointer loaded from memory",
          "[opt][pointer-analysis]") {
    test::passTest("ptranalysis, instcombine, sroa",
                   R"(
@p = global ptr nullptr
func i1 @test() {
  %entry:
    %i.addr = alloca i64, i32 1
    %p.2 = load ptr, ptr @p
    %eq.2 = ucmp eq ptr %p.2, ptr %i.addr
    return i1 %eq.2
}
)",
                   R"(
func i1 @test() {
    %entry:
    return i1 0
}
)");
}

TEST_CASE("Compare alloca to pointer argument", "[opt][pointer-analysis]") {
    test::passTest("ptranalysis, instcombine, sroa",
                   R"(
func i1 @test-_Ps64(ptr %0) {
    %entry:
    %i.addr = alloca i64, i32 1
    %eq = ucmp eq ptr %i.addr, ptr %0
    return i1 %eq
}
)",
                   R"(
func i1 @test-_Ps64(ptr %0) {
    %entry:
    return i1 0
}
)");
}
