#include <catch2/catch_test_macros.hpp>

#include "Opt/PassTest.h"
#include "Opt/Passes.h"

using namespace scatha;

TEST_CASE("MemToReg - 1", "[opt][mem-to-reg]") {
    test::passTest(&opt::memToReg,
                   R"(
func i64 @f(i64) {
  %entry:
    %i = alloca i64
    %result = alloca i64
    %j = alloca i64
    %tmp = alloca i64
    %tmp.4 = alloca i64
    store ptr %i, i64 %0
    store ptr %result, i64 0
    store ptr %j, i64 1
    goto label %loop.header

  %loop.header:
    %j.1 = load i64, ptr %j
    %cmp.result = scmp leq i64 %j.1, i64 5
    branch i1 %cmp.result, label %loop.body, label %loop.end

  %loop.body:
    %result.1 = load i64, ptr %result
    %expr.result = urem i64 %result.1, i64 2
    %cmp.result.1 = scmp eq i64 %expr.result, i64 0
    branch i1 %cmp.result.1, label %then, label %else

  %loop.end:
    %result.4 = load i64, ptr %result
    return i64 %result.4

  %then:
    %result.2 = load i64, ptr %result
    %j.2 = load i64, ptr %j
    %expr.result.1 = add i64 %result.2, i64 %j.2
    store ptr %tmp, i64 %expr.result.1
    %tmp.1 = load i64, ptr %tmp
    %tmp.2 = load i64, ptr %tmp
    store ptr %result, i64 %tmp.2
    %tmp.3 = load i64, ptr %result
    goto label %if.end

  %else:
    %result.3 = load i64, ptr %result
    %expr.result.2 = mul i64 2, i64 %result.3
    %j.3 = load i64, ptr %j
    %expr.result.3 = sub i64 %expr.result.2, i64 %j.3
    store ptr %tmp.4, i64 %expr.result.3
    %tmp.5 = load i64, ptr %tmp.4
    store ptr %result, i64 %tmp.5
    %tmp.6 = load i64, ptr %result
    goto label %if.end

  %if.end:
    %++.value.1 = load i64, ptr %j
    %++.result = add i64 %++.value.1, i64 1
    store ptr %j, i64 %++.result
    goto label %loop.header
})",
                   R"(
func i64 @f(i64 %0) {
  %entry:
    goto label %loop.header

  %loop.header:               // preds: entry, if.end
    %j.0 = phi i64 [label %entry : 1], [label %if.end : %++.result]
    %result.0 = phi i64 [label %entry : 0], [label %if.end : %result.5]
    %cmp.result = scmp leq i64 %j.0, i64 5
    branch i1 %cmp.result, label %loop.body, label %loop.end

  %loop.body:                 // preds: loop.header
    %expr.result = urem i64 %result.0, i64 2
    %cmp.result.1 = scmp eq i64 %expr.result, i64 0
    branch i1 %cmp.result.1, label %then, label %else

  %loop.end:                  // preds: loop.header
    return i64 %result.0

  %then:                      // preds: loop.body
    %expr.result.1 = add i64 %result.0, i64 %j.0
    goto label %if.end

  %else:                      // preds: loop.body
    %expr.result.2 = mul i64 2, i64 %result.0
    %expr.result.3 = sub i64 %expr.result.2, i64 %j.0
    goto label %if.end

  %if.end:                    // preds: then, else
    %result.5 = phi i64 [label %then : %expr.result.1], [label %else : %expr.result.3]
    %++.result = add i64 %j.0, i64 1
    goto label %loop.header
})");
}

TEST_CASE("MemToReg - 2", "[opt][mem-to-reg]") {
    test::passTest(&opt::memToReg,
                   R"(
func i64 @f(i64) {
  %entry:
    %n = alloca i64
    %i = alloca i64
    store ptr %i, i64 %0
    %i.1 = load i64, ptr %i
    %cmp.result = scmp grt i64 %i.1, i64 0
    branch i1 %cmp.result, label %then, label %if.end

  %then:
    store ptr %n, i64 1
    %tmp = load i64, ptr %n
    goto label %if.end

  %if.end:
    %n.1 = load i64, ptr %n
    return i64 %n.1
})",
                   R"(
func i64 @f(i64 %0) {
  %entry:
    %cmp.result = scmp grt i64 %0, i64 0
    branch i1 %cmp.result, label %then, label %if.end

  %then:                      // preds: entry
    goto label %if.end

  %if.end:                    // preds: entry, then
    %n.0 = phi i64 [label %entry : undef], [label %then : 1]
    return i64 %n.0
})");
}

TEST_CASE("Unreachable load", "[opt][mem-to-reg]") {
    test::passTest(&opt::memToReg,
                   R"(
func i64 @foo() {
  %entry:
    %0 = alloca i64, i32 1
    return i64 0

  %block:
    %1 = load i64, ptr %0
    return i64 %1
}
)",
                   R"(
func i64 @foo() {
  %entry:
    return i64 0

  %block:
    return i64 undef
}
)");
}
