#include <bit>
#include <cmath>

#include <catch2/catch_test_macros.hpp>

#include "EndToEndTests/PassTesting.h"

using namespace scatha;
using namespace test;

TEST_CASE("abs_f64", "[end-to-end][math]") {
    runReturnsTest(std::bit_cast<uint64_t>(std::abs(-1.0)), R"(
fn main() { return reinterpret<int>(__builtin_abs_f64(-1.0)); })");
}

TEST_CASE("abs_f32", "[end-to-end][math]") {
    runReturnsTest(std::bit_cast<uint32_t>(std::abs(-1.0f)), R"(
fn main() { return reinterpret<u32>(__builtin_abs_f32(-1.0)); })");
}

TEST_CASE("fract_f64", "[end-to-end][math]") {
    runReturnsTest(std::bit_cast<uint64_t>(.5), R"(
fn main() { return reinterpret<int>(__builtin_fract_f64(1.5)); })");
}

TEST_CASE("fract_f64 negative", "[end-to-end][math]") {
    runReturnsTest(std::bit_cast<uint64_t>(.5), R"(
fn main() { return reinterpret<int>(__builtin_fract_f64(-1.5)); })");
}

TEST_CASE("fract_f32", "[end-to-end][math]") {
    runReturnsTest(std::bit_cast<uint32_t>(.5f), R"(
fn main() { return reinterpret<u32>(__builtin_fract_f32(1.5)); })");
}

TEST_CASE("floor_f64", "[end-to-end][math]") {
    runReturnsTest(std::bit_cast<uint64_t>(1.0), R"(
fn main() { return reinterpret<int>(__builtin_floor_f64(1.5)); })");
}

TEST_CASE("ceil_f64", "[end-to-end][math]") {
    runReturnsTest(std::bit_cast<uint64_t>(2.0), R"(
fn main() { return reinterpret<int>(__builtin_ceil_f64(1.5)); })");
}
