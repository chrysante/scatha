#include <catch2/catch_test_macros.hpp>

#include "IR/CFG.h"
#include "IR/Context.h"
#include "IR/IRParser.h"
#include "IR/Module.h"
#include "Opt/PassTest.h"
#include "Opt/Passes.h"

using namespace scatha;
using namespace opt;
using namespace ir;

TEST_CASE("Unreachable blocks", "[opt][gvn]") {
    test::passTest(&opt::globalValueNumbering,
                   R"(
func void @test(i1 %cond) {
  %entry:
    return

  %unreach.begin:
    %sum = add i64 1, i64 2
    branch i1 %cond, label %unreach.then, label %unreach.end

  %unreach.then:
    goto label %unreach.end

  %unreach.end:
    %sum2 = add i64 1, i64 2
    return
})",
                   R"(
func void @test(i1 %cond) {
  %entry:
    return

  %unreach.begin:
    %sum = add i64 1, i64 2
    branch i1 %cond, label %unreach.then, label %unreach.end

  %unreach.then:
    goto label %unreach.end

  %unreach.end:
    %sum2 = phi i64 [label %unreach.begin: %sum], [label %unreach.then: %sum]
    return
}
)");
}
