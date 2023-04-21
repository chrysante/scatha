#include <Catch/Catch2.hpp>

#include "Opt/InstCombine.h"
#include "test/Opt/PassTest.h"

using namespace scatha;
using namespace opt;
using namespace ir;

TEST_CASE("InstCombine - Arithmetic - 1", "[opt][inst-combine]") {
    test::passTest(&opt::instCombine,
                   R"(
func i32 @main(i32 %0) {
  %entry:
    %1 = add i32 1, i32 %0
    %2 = add i32 1, i32 %1
    %3 = add i32 1, i32 %2
    %4 = sub i32 %3, i32 2
    %5 = add i32 5, i32 %4
    return i32 %5
})",
                   R"(
func i32 @main(i32 %0) {
  %entry:
    %1 = add i32 %0, i32 6
    return i32 %1
})");
}

TEST_CASE("InstCombine - Arithmetic - 2", "[opt][inst-combine]") {
    test::passTest(&opt::instCombine,
                   R"(
func i32 @main(i32 %0) {
  %entry:
    %1 = add i32 1, i32 %0
    %2 = add i32 1, i32 %1
    %3 = add i32 1, i32 %2
    %4 = sub i32 %3, i32 2
    return i32 %4
})",
                   R"(
func i32 @main(i32 %0) {
  %entry:
    %1 = add i32 %0, i32 3
    return i32 %1
})");
}

TEST_CASE("InstCombine - Arithmetic - 3", "[opt][inst-combine]") {
    test::passTest(&opt::instCombine,
                   R"(
func i32 @main(i32 %0) {
  %entry:
    %z = sub i32 %0, i32 %0
    %i = sdiv i32 %0, i32 %z
    return i32 %i
})",
                   R"(
func i32 @main(i32 %0) {
  %entry:
    return i32 undef
})");
}

TEST_CASE("InstCombine - InsertValue - 1", "[opt][inst-combine]") {
    test::passTest(&opt::instCombine,
                   R"(
struct @Y { i64, f64, i32 }
struct @X { i64, f64, @Y }
func @X @main(@X %xbase, @Y %ybase) {
  %entry:
    %x.0 = extract_value @X %xbase, 0
    %x.1 = extract_value @X %xbase, 1
    %y.1 = extract_value @Y %ybase, 1
  
    %r.0 = insert_value @X undef, i64 %x.0, 0
    %r.1 = insert_value @X %r.0,  f64 %x.1, 1
    %r.2 = insert_value @X %r.1,  i64 1,    2, 0
    %r.3 = insert_value @X %r.2,  f64 %y.1, 2, 1
    %r.4 = insert_value @X %r.3,  i32 0,    2, 2
    
    return @X %r.4
})",
                   R"(
struct @Y { i64, f64, i32 }
struct @X { i64, f64, @Y }
func @X @main(@X %xbase, @Y %ybase) {
  %entry:
    %iv.0 = insert_value @Y %ybase, i64 1, 0
    %iv.1 = insert_value @Y %iv.0, i32 0, 2
    %iv.2 = insert_value @X %xbase, @Y %iv.1, 2
    return @X %iv.2
})");
}

TEST_CASE("InstCombine - InsertValue - 2", "[opt][inst-combine]") {
    test::passTest(&opt::instCombine,
                   R"(
struct @Y {
  i64, f64, i64
}
struct @X {
  @Y
}
func @X @main(@X %0, @X %1, @X %2) {
  %entry:
    %x.0_0_0.2 = extract_value @X %0, 0, 0
    %x.0_0_1.2 = extract_value @X %0, 0, 1
    %x.0_0_2.2 = extract_value @X %0, 0, 2
    %y.0_0.2 = extract_value @X %1, 0
    %z.0_0_0.2 = extract_value @X %2, 0, 0
    %z.0_0_1.2 = extract_value @X %2, 0, 1
    %z.0_0_2.2 = extract_value @X %2, 0, 2
    %z.0_0_0.4 = extract_value @Y %y.0_0.2, 0
    %z.0_0_1.4 = extract_value @Y %y.0_0.2, 1
    %z.0_0_2.4 = extract_value @Y %y.0_0.2, 2
    %z.5 = insert_value @X undef, i64 %z.0_0_0.4, 0, 0
    %z.9 = insert_value @X %z.5, f64 %x.0_0_1.2, 0, 1
    %z.13 = insert_value @X %z.9, i64 %z.0_0_2.4, 0, 2
    return @X %z.13
})",
                   R"(
struct @Y {
  i64, f64, i64
}
struct @X {
  @Y
}
func @X @main(@X %0, @X %1, @X %2) {
  %entry:
    %x.0_0_1.2 = extract_value @X %0, 0, 1
    %y.0_0.2 = extract_value @X %1, 0
    %iv.0 = insert_value @Y %y.0_0.2, f64 %x.0_0_1.2, 1
    %iv.2 = insert_value @X undef, @Y %iv.0, 0
    return @X %iv.2
})");
}

TEST_CASE("InstCombine - InsertValue - 3", "[opt][inst-combine]") {
    test::passTest(&opt::instCombine,
                   R"(
struct @Y {
    i64,f64
}
struct @X {
  @Y, @Y, i64
}
func @X @main(@X %0) {
  %entry:
    %x.0_0_0.2 = extract_value @X %0, 0, 0
    %x.0_0_1.2 = extract_value @X %0, 0, 1
    %x.0_1_0.2 = extract_value @X %0, 1, 0
    %x.0_1_1.2 = extract_value @X %0, 1, 1
    %x.0_2.2 = extract_value @X %0, 2
    %r.5 = insert_value @X undef, i64 %x.0_0_0.2, 0, 0
    %r.9 = insert_value @X %r.5, f64 %x.0_0_1.2, 0, 1
    %r.13 = insert_value @X %r.9, i64 %x.0_1_0.2, 1, 0
    %r.17 = insert_value @X %r.13, f64 %x.0_1_1.2, 1, 1
    %r.21 = insert_value @X %r.17, i64 %x.0_2.2, 2
    return @X %r.21
})",
                   R"(
struct @Y {
    i64,f64
}
struct @X {
  @Y, @Y, i64
}
func @X @main(@X %0) {
  %entry:
    return @X %0
})");
}
