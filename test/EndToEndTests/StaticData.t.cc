#include <Catch/Catch2.hpp>

#include "test/EndToEndTests/BasicCompiler.h"

using namespace scatha;

TEST_CASE("Static data", "[end-to-end][static-data]") {
    test::checkIRReturns(7, R"(
@const_data = [i32, 3] [i32 1, i32 2, i32 3]

@other_data = i32 1

func i32 @main() {
  %entry:
    %1 = load i32, ptr @other_data
    %p = getelementptr inbounds i32, ptr @const_data, i32 0
    %t0 = load i32, ptr %p
    %q = getelementptr inbounds i32, ptr @const_data, i32 1
    %t1 = load i32, ptr %q
    %r = getelementptr inbounds i32, ptr @const_data, i32 2
    %t2 = load i32, ptr %r
    %s0 = add i32 %t0, i32 %t1
    %s1 = add i32 %s0, i32 %t2
    %s2 = add i32 %s1, i32 %1
    return i32 %s2
})");
}
