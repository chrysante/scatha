#include <Catch/Catch2.hpp>

#include "test/EndToEndTests/PassTesting.h"

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

TEST_CASE("Float conversion", "[end-to-end]") {
    test::checkIRReturns(utl::bit_cast<u64>(static_cast<double>(3.0f / 2.0f)),
                         R"(
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

TEST_CASE("Narrowing constant conversions in function calls", "[end-to-end]") {
    test::checkCompiles(R"(
fn f(b: byte) {}
fn g(b: u8) {}
fn main(n: int) -> byte {
    f(100);
    g(100);
})");
}

TEST_CASE("String conversions to int", "[end-to-end]") {
    test::checkReturns(123, R"(
fn main() {
    var value: int;
    if __builtin_strtos64(value, "123", 10) {
        return value;
    }
    return -1;
})");
    test::checkReturns(u64(-123), R"(
fn main() {
    var value: int;
    if __builtin_strtos64(value, "-123", 10) {
        return value;
    }
    return -1;
})");
    test::checkReturns(256, R"(
fn main() {
    var value: int;
    if __builtin_strtos64(value, "100", 16) {
        return value;
    }
    return -1;
})");
    test::checkReturns(0b1010, R"(
fn main() {
    var value: int;
    if __builtin_strtos64(value, "1010", 2) {
        return value;
    }
    return -1;
})");
    test::checkReturns(u64(-1), R"(
fn main() {
    var value: int;
    if __builtin_strtos64(value, "abc", 10) {
        return value;
    }
    return -1;
})");
}

TEST_CASE("String conversions to double", "[end-to-end]") {
    test::checkReturns(utl::bit_cast<u64>(123.0), R"(
fn main() {
    var value: double;
    if __builtin_strtof64(value, "123") {
        return value;
    }
    return 0.0;
})");
    test::checkReturns(utl::bit_cast<u64>(0.0), R"(
fn main() {
    var value: double;
    if __builtin_strtof64(value, "0.0") {
        return value;
    }
    return -1.0;
})");
    test::checkReturns(utl::bit_cast<u64>(-1.0), R"(
fn main() {
    var value: double;
    if __builtin_strtof64(value, "-1") {
        return value;
    }
    return 0.0;
})");
    test::checkReturns(utl::bit_cast<u64>(0.0), R"(
fn main() {
    var value: double;
    if __builtin_strtof64(value, "abc") {
        return value;
    }
    return 0.0;
})");
}
