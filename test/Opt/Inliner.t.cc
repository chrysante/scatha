#include <catch2/catch_test_macros.hpp>

#include "Opt/PassTest.h"
#include "Opt/Passes.h"

using namespace scatha;
using namespace opt;
using namespace ir;

TEST_CASE("Inliner - Regression - 1", "[opt][inliner]") {
    test::passTest(&opt::inlineFunctions, &opt::defaultPass,
                   R"(
func void @f() {
  %entry:
    return
}

func void @g() {
  %entry:
    return
}

func void @h() {
  %entry:
    return
}

func void @main() {
  %entry:
    call void @f
    call void @g
    call void @f
    call void @h
    return
})",
                   R"(
func void @f() {
  %entry:
    return
}

func void @g() {
  %entry:
    return
}

func void @h() {
  %entry:
    return
}

func void @main() {
  %entry:
    return
})");
}
