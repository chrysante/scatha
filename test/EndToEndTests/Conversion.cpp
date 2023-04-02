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
    
TEST_CASE("Float converstion", "[end-to-end]") {
    test::checkIRReturns(utl::bit_cast<u64>(static_cast<double>(3.0f / 2.0f)), R"(
func f64 @main() {
  %entry:
    %q = fdiv f32 3.0, f32 2.0
    %r = fext f32 %q to f64
    return f64 %r
})");
}

TEST_CASE("Bitcast", "[end-to-end]") {
    test::checkIRReturns(utl::bit_cast<u64>(11.0 + 0.1), R"(
func i64 @main() {
  %entry:
    %a = fadd f64 11.0, f64 0.1
    %r = bitcast f64 %a to i64
    return i64 %r
})");
}
