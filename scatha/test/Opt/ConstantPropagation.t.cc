#include <catch2/catch_test_macros.hpp>

#include "Opt/PassTest.h"
#include "Opt/Passes.h"

using namespace scatha;
using namespace opt;
using namespace ir;

TEST_CASE("Bitcast folding", "[opt][propconst]") {
    test::passTest(&opt::propagateConstants,
                   R"(
func i64 @main() {
  %entry:
    %a = bitcast i64 0 to f64
    %b = bitcast f64 %a to i64
    return i64 %b
})",
                   R"(
func i64 @main() {
  %entry:
    return i64 0
})");
}
