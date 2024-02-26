#include <catch2/catch_test_macros.hpp>

#include "IR/CFG.h"
#include "IR/Context.h"
#include "IR/Type.h"
#include "Opt/Common.h"
#include "Opt/PassTest.h"

using namespace scatha;
using namespace opt;
using namespace ir;

TEST_CASE("Remove critical edges", "[opt][common]") {
    test::passTest(&opt::splitCriticalEdges,
                   R"(
func void @main() {
  %entry:
    branch i1 undef, label %if, label %end
  %if:
    goto label %end
  %end:
    return
})",
                   R"(
func void @main() {
  %entry:
    branch i1 undef, label %if, label %tmp.0
  %if:
    goto label %end
  %tmp.0:
    goto label %end
  %end:
    return
})");
}
