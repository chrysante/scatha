#include <Catch/Catch2.hpp>

#include "test/EndToEndTests/BasicCompiler.h"

using namespace scatha;

TEST_CASE("Sign extend", "[end-to-end]") {
    test::checkIRReturns(0xFFFF'FFFF'FFFF'FFFE, R"(
func i64 @main() {
  %entry:
    %q = sdiv i32 100, i32 -5
    %r = srem i32 %q, i32 3
    %re = sext i32 %r to i64
    return i64 %re
})");
}

TEST_CASE("Zero extend", "[end-to-end]") {
    test::checkIRReturns(0x0000'0000'FFFF'FFFE, R"(
func i64 @main() {
  %entry:
    %q = sdiv i32 100, i32 -5
    %r = srem i32 %q, i32 3
    %re = zext i32 %r to i64
    return i64 %re
})");
}
