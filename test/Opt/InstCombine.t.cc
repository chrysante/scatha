#include <catch2/catch_test_macros.hpp>

#include "Opt/PassTest.h"
#include "Opt/Passes.h"
#include "IR/IRParser.h"
#include "IR/Context.h"
#include "IR/Module.h"
#include "IR/CFG.h"

using namespace scatha;
using namespace opt;
using namespace ir;

TEST_CASE("InstCombine - Arithmetic - 1", "[opt][inst-combine]") {
    test::passTest(&opt::instCombine,
                   R"(
func i32 @main(i32 %0) {
  %entry:
    %1 = add i32 1, i32 %0
    %2 = add i32 1, i32 %1
    %3 = add i32 1, i32 %2
    %4 = sub i32 %3, i32 2
    %5 = add i32 5, i32 %4
    return i32 %5
})",
                   R"(
func i32 @main(i32 %0) {
  %entry:
    %1 = add i32 %0, i32 6
    return i32 %1
})");
}

TEST_CASE("InstCombine - Arithmetic - 2", "[opt][inst-combine]") {
    test::passTest(&opt::instCombine,
                   R"(
func i32 @main(i32 %0) {
  %entry:
    %1 = add i32 1, i32 %0
    %2 = add i32 1, i32 %1
    %3 = add i32 1, i32 %2
    %4 = sub i32 %3, i32 2
    return i32 %4
})",
                   R"(
func i32 @main(i32 %0) {
  %entry:
    %1 = add i32 %0, i32 3
    return i32 %1
})");
}

TEST_CASE("InstCombine - Arithmetic - 3", "[opt][inst-combine]") {
    test::passTest(&opt::instCombine,
                   R"(
func i32 @main(i32 %0) {
  %entry:
    %z = sub i32 %0, i32 %0
    %i = sdiv i32 %0, i32 %z
    return i32 %i
})",
                   R"(
func i32 @main(i32 %0) {
  %entry:
    return i32 undef
})");
}

TEST_CASE("InstCombine - Arithmetic - 4", "[opt][inst-combine]") {
    test::passTest(&opt::instCombine,
                   R"(
func i64 @main(i64 %0, i64 %1) {
  %entry:
    %2 = neg i64 %1
    %3 = add i64 %0, i64 %2
    return i64 %3
})",
                   R"(
func i64 @main(i64 %0, i64 %1) {
  %entry:
    %3 = sub i64 %0, i64 %1
    return i64 %3
})");
}

TEST_CASE("InstCombine - Arithmetic - 5", "[opt][inst-combine]") {
    test::passTest(&opt::instCombine,
                   R"(
func i64 @main(i64 %0, i64 %1) {
  %entry:
    %2 = neg i64 %0
    %3 = add i64 %2, i64 %1
    return i64 %3
})",
                   R"(
func i64 @main(i64 %0, i64 %1) {
  %entry:
    %3 = sub i64 %1, i64 %0
    return i64 %3
})");
}

TEST_CASE("InstCombine - Arithmetic - 6", "[opt][inst-combine]") {
    test::passTest(&opt::instCombine,
                   R"(
func i64 @main(i64 %0, i64 %1) {
  %entry:
    %2 = neg i64 %1
    %3 = sub i64 %0, i64 %2
    return i64 %3
})",
                   R"(
func i64 @main(i64 %0, i64 %1) {
  %entry:
    %3 = add i64 %0, i64 %1
    return i64 %3
})");
}

TEST_CASE("InstCombine - Arithmetic - 7", "[opt][inst-combine]") {
    test::passTest(&opt::instCombine,
                   R"(
func i64 @main(i64 %0, i64 %1) {
  %entry:
    %2 = neg i64 %0
    %3 = neg i64 %1
    %4 = sub i64 %2, i64 %3
    return i64 %4
})",
                   R"(
func i64 @main(i64 %0, i64 %1) {
  %entry:
    %4 = sub i64 %1, i64 %0
    return i64 %4
})");
}

TEST_CASE("InstCombine - InsertValue - 1", "[opt][inst-combine]") {
    test::passTest(&opt::instCombine,
                   R"(
struct @Y { i64, f64, i32 }
struct @X { i64, f64, @Y }
func @X @main(@X %xbase, @Y %ybase) {
  %entry:
    %x.0 = extract_value @X %xbase, 0
    %x.1 = extract_value @X %xbase, 1
    %y.1 = extract_value @Y %ybase, 1
  
    %r.0 = insert_value @X undef, i64 %x.0, 0
    %r.1 = insert_value @X %r.0,  f64 %x.1, 1
    %r.2 = insert_value @X %r.1,  i64 1,    2, 0
    %r.3 = insert_value @X %r.2,  f64 %y.1, 2, 1
    %r.4 = insert_value @X %r.3,  i32 0,    2, 2
    
    return @X %r.4
})",
                   R"(
struct @Y { i64, f64, i32 }
struct @X { i64, f64, @Y }
func @X @main(@X %xbase, @Y %ybase) {
  %entry:
    %iv.0 = insert_value @Y %ybase, i64 1, 0
    %iv.1 = insert_value @Y %iv.0, i32 0, 2
    %iv.2 = insert_value @X %xbase, @Y %iv.1, 2
    return @X %iv.2
})");
}

TEST_CASE("InstCombine - InsertValue - 2", "[opt][inst-combine]") {
    test::passTest(&opt::instCombine,
                   R"(
struct @Y {
  i64, f64, i64
}
struct @X {
  @Y
}
func @X @main(@X %0, @X %1, @X %2) {
  %entry:
    %x.0_0_0.2 = extract_value @X %0, 0, 0
    %x.0_0_1.2 = extract_value @X %0, 0, 1
    %x.0_0_2.2 = extract_value @X %0, 0, 2
    %y.0_0.2 = extract_value @X %1, 0
    %z.0_0_0.2 = extract_value @X %2, 0, 0
    %z.0_0_1.2 = extract_value @X %2, 0, 1
    %z.0_0_2.2 = extract_value @X %2, 0, 2
    %z.0_0_0.4 = extract_value @Y %y.0_0.2, 0
    %z.0_0_1.4 = extract_value @Y %y.0_0.2, 1
    %z.0_0_2.4 = extract_value @Y %y.0_0.2, 2
    %z.5 = insert_value @X undef, i64 %z.0_0_0.4, 0, 0
    %z.9 = insert_value @X %z.5, f64 %x.0_0_1.2, 0, 1
    %z.13 = insert_value @X %z.9, i64 %z.0_0_2.4, 0, 2
    return @X %z.13
})",
                   R"(
struct @Y {
  i64, f64, i64
}
struct @X {
  @Y
}
func @X @main(@X %0, @X %1, @X %2) {
  %entry:
    %x.0_0_1.2 = extract_value @X %0, 0, 1
    %y.0_0.2 = extract_value @X %1, 0
    %iv.0 = insert_value @Y %y.0_0.2, f64 %x.0_0_1.2, 1
    %iv.2 = insert_value @X undef, @Y %iv.0, 0
    return @X %iv.2
})");
}

TEST_CASE("InstCombine - InsertValue - 3", "[opt][inst-combine]") {
    test::passTest(&opt::instCombine,
                   R"(
struct @Y {
    i64,f64
}
struct @X {
  @Y, @Y, i64
}
func @X @main(@X %0) {
  %entry:
    %x.0_0_0.2 = extract_value @X %0, 0, 0
    %x.0_0_1.2 = extract_value @X %0, 0, 1
    %x.0_1_0.2 = extract_value @X %0, 1, 0
    %x.0_1_1.2 = extract_value @X %0, 1, 1
    %x.0_2.2 = extract_value @X %0, 2
    %r.5 = insert_value @X undef, i64 %x.0_0_0.2, 0, 0
    %r.9 = insert_value @X %r.5, f64 %x.0_0_1.2, 0, 1
    %r.13 = insert_value @X %r.9, i64 %x.0_1_0.2, 1, 0
    %r.17 = insert_value @X %r.13, f64 %x.0_1_1.2, 1, 1
    %r.21 = insert_value @X %r.17, i64 %x.0_2.2, 2
    return @X %r.21
})",
                   R"(
struct @Y {
    i64,f64
}
struct @X {
  @Y, @Y, i64
}
func @X @main(@X %0) {
  %entry:
    return @X %0
})");
}

TEST_CASE("InstCombine - ExtractValue from phi", "[opt][inst-combine]") {
    test::passTest(&opt::instCombine,
                   R"(
struct @X { i32, i32 }

func i32 @main(i1 %cond) {
  %entry:
    %A.0 = insert_value @X undef, i32 1, 0
    %A.1 = insert_value @X %A.0, i32 2, 1
    %B.0 = insert_value @X undef, i32 3, 0
    %B.1 = insert_value @X %B.0, i32 4, 1
    branch i1 %cond, label %then, label %else

  %then:                      // preds: entry
    goto label %then.continue

  %then.continue:             // preds: then
    goto label %end

  %else:                      // preds: entry
    goto label %end

  %end:                       // preds: then.continue, else
    %C = phi @X [label %then.continue : %A.1], [label %else : %B.1]
    branch i1 %cond, label %then.1, label %else.1

  %then.1:                    // preds: end
    goto label %end.1

  %else.1:                    // preds: end
    branch i1 %cond, label %then.2, label %else.2

  %then.2:                    // preds: else.1
    goto label %end.1

  %else.2:                    // preds: else.1
    goto label %end.1

  %end.1:                     // preds: then.1, then.2, else.2
    %result = extract_value @X %C, 1
    return i32 %result
}
)",
                   R"(
struct @X { i32, i32 }

func i32 @main(i1 %cond) {
  %entry:
    branch i1 %cond, label %then, label %else

  %then:                      // preds: entry
    goto label %then.continue

  %then.continue:             // preds: then
    goto label %end

  %else:                      // preds: entry
    goto label %end

  %end:                       // preds: then.continue, else
    %result.phi = phi i32 [label %then.continue : 2], [label %else : 4]
    branch i1 %cond, label %then.1, label %else.1

  %then.1:                    // preds: end
    goto label %end.1

  %else.1:                    // preds: end
    branch i1 %cond, label %then.2, label %else.2

  %then.2:                    // preds: else.1
    goto label %end.1

  %else.2:                    // preds: else.1
    goto label %end.1

  %end.1:                     // preds: then.1, then.2, else.2
    return i32 %result.phi
}
)");
}

TEST_CASE("Devirtualization", "[opt][inst-combine]") {
    test::passTest(&opt::instCombine,
                   R"(
@vtable = constant [ptr, 2] [ptr @f1, ptr @f2]

func i32 @main() {
%entry:
    %p = getelementptr inbounds ptr, ptr @vtable, i32 1
    %f = load ptr, ptr %p
    %r = call i32 %f
    return i32 %r
}

func i32 @f1() {
%entry:
    return i32 0
}

func i32 @f2() {
%entry:
    return i32 1
})",
                   R"(
@vtable = constant [ptr, 2] [ptr @f1, ptr @f2]

func i32 @main() {
%entry:
    %r = call i32 @f2
    return i32 %r
}

func i32 @f1() {
%entry:
    return i32 0
}

func i32 @f2() {
%entry:
    return i32 1
})");
}

TEST_CASE("InstCombine pointer comparison", "[opt][inst-combine]") {
    auto [ctx, mod] = ir::parse(R"(
func i1 @test() {
%entry:
    %a = alloca i32, i32 1
    %res = ucmp eq ptr %a, ptr nullptr
    return i1 %res
})").value();
    auto& F = mod.front();
    opt::pointerAnalysis(ctx, F);
    opt::instCombine(ctx, F);
    auto& entry = F.front();
    auto* retval = cast<Return const*>(entry.terminator())->value();
    CHECK(retval == ctx.intConstant(false, 1));
}
