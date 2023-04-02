#include <Catch/Catch2.hpp>

#include "test/EndToEndTests/BasicCompiler.h"

using namespace scatha;

TEST_CASE("Sign extend", "[end-to-end]") {
    test::checkIRReturns(1, R"(
func i1 @main() {
  %entry:
    %res = sdiv i32 10, i32 -5
    %cond = ucmp eq i32 %res, i32 -2
    return i1 %cond
})");
}
