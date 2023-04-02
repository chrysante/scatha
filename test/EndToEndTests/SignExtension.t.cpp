#include <Catch/Catch2.hpp>

#include "test/EndToEndTests/BasicCompiler.h"

using namespace scatha;

TEST_CASE("Sign extend", "[end-to-end]") {
    test::checkIRReturns(1, R"(
func i1 @main() {
  %entry:
    %q = sdiv i32 100, i32 -5
    %r = srem i32 %q, i32 3
    %cond = ucmp eq i32 %r, i32 -2
    return i1 %cond
})");
}
