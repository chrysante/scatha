#include <Catch/Catch2.hpp>

#include "Opt/Passes.h"
#include "test/Opt/PassTest.h"

using namespace scatha;
using namespace opt;
using namespace ir;

TEST_CASE("SROA - 1", "[opt][sroa]") {
    test::passTest(&opt::sroa,
                   R"(
struct @X {
  i64, i64
}
func i64 @main() {
  %entry:
    %data = alloca @X, i32 10
    
    # Make variable x
    %x.tmp = insert_value @X undef, i64 1, 0
    %x = insert_value @X %x.tmp, i64 2, 1
    
    # Make variable y
    %y.tmp = insert_value @X undef, i64 1, 0
    %y = insert_value @X %y.tmp, i64 2, 1
    
    # Store x into array index 3
    %data.at.3 = getelementptr inbounds @X, ptr %data, i32 3
    store ptr %data.at.3, @X %x
    
    # Store y into array index 5
    %data.at.5 = getelementptr inbounds @X, ptr %data, i32 5
    store ptr %data.at.5, @X %y
    
    # Load second data member from index 3
    %member.1 = getelementptr inbounds @X, ptr %data.at.3, i32 0, 1
    %lhs = load i64, ptr %member.1
    
    # Load first data member from index 5
    %member.0 = getelementptr inbounds @X, ptr %data.at.5, i32 0, 0
    %rhs = load i64, ptr %member.0
    
    # Sum lhs and rhs
    %res = add i64 %lhs, i64 %rhs
    
    return i64 %res
})",
                   R"(
struct @X {
  i64, i64
}
func i64 @main() {
  %entry:
    %data.1 = alloca i64, i32 1
    %data.3 = alloca i64, i32 1
    %data.5 = alloca i64, i32 1
    %data.7 = alloca i64, i32 1

    %x.tmp = insert_value @X undef, i64 1, 0
    %x = insert_value @X %x.tmp, i64 2, 1

    %y.tmp = insert_value @X undef, i64 1, 0
    %y = insert_value @X %y.tmp, i64 2, 1

    %data.9 = extract_value @X %x, 0
    store ptr %data.1, i64 %data.9

    %data.11 = extract_value @X %x, 1
    store ptr %data.3, i64 %data.11

    %data.13 = extract_value @X %y, 0
    store ptr %data.5, i64 %data.13

    %data.15 = extract_value @X %y, 1
    store ptr %data.7, i64 %data.15

    %lhs = load i64, ptr %data.3
    %rhs = load i64, ptr %data.5
    %res = add i64 %lhs, i64 %rhs

    return i64 %res
})");
}

TEST_CASE("SROA - 2", "[opt][sroa]") {
    test::passTest(&opt::sroa,
                   R"(
struct @X {
  i1, i64
}
func i64 @f(@X %0) {
  %entry:
    %r = alloca i64, i32 1
    %x = alloca @X, i32 1
    store ptr %x, @X %0
    %x.1 = getelementptr inbounds @X, ptr %x, i32 0, 1
    %x.1.value = load i64, ptr %x.1
    store ptr %r, i64 %x.1.value
    %ret = load i64, ptr %r
    return i64 %ret
})",
                   R"(
struct @X {
  i1, i64
}
func i64 @f(@X %0) {
  %entry:
    %r = alloca i64, i32 1
    %x.2 = alloca i1, i32 1
    %x.4 = alloca i64, i32 1
    %x.6 = extract_value @X %0, 0
    store ptr %x.2, i1 %x.6
    %x.8 = extract_value @X %0, 1
    store ptr %x.4, i64 %x.8
    %x.1.value = load i64, ptr %x.4
    store ptr %r, i64 %x.1.value
    %ret = load i64, ptr %r
    return i64 %ret
})");
}

TEST_CASE("SROA - 3", "[opt][sroa]") {
    test::passTest(&opt::sroa,
                   R"(
struct @X {
  i64, i64, i64
}
func i64 @main(i1 %cond) {
  %entry:
    %a = alloca @X, i32 10
    %p.0 = getelementptr inbounds @X, ptr %a, i32 3
    %i = insert_value @X undef, i64 3, 2
    store ptr %p.0, @X %i
    branch i1 %cond, label %if, label %then

  %if:
    %p.1 = getelementptr inbounds @X, ptr %a, i32 3   # Same as p.0
    %p.2 = getelementptr inbounds @X, ptr %p.1, i32 0, 0
    store ptr %p.2, i64 1
    %p.3 = getelementptr inbounds @X, ptr %p.1, i32 0, 1
    store ptr %p.3, i64 2
    goto label %end

  %then:
    %p.4 = getelementptr inbounds @X, ptr %a, i32 3   # Same as p.0
    %p.5 = getelementptr inbounds @X, ptr %p.4, i32 0, 0
    store ptr %p.5, i64 5
    %p.6 = getelementptr inbounds @X, ptr %p.4, i32 0, 1
    store ptr %p.6, i64 6
    goto label %end

  %end:
    %p.7 = getelementptr inbounds @X, ptr %a, i32 3, 0
    %p.8 = getelementptr inbounds @X, ptr %a, i32 3, 1
    %p.9 = getelementptr inbounds @X, ptr %a, i32 3, 2
    %x = load i64, ptr %p.7
    %y = load i64, ptr %p.8
    %z = load i64, ptr %p.9
    %r.0 = add i64 %x, i64 %y
    %r.1 = add i64 %r.0, i64 %z
    return i64 %r.1
})",
                   R"(
struct @X {
  i64,
  i64,
  i64
}
func i64 @main(i1 %cond) {
  %entry:
    %a.1 = alloca i64, i32 1
    %a.3 = alloca i64, i32 1
    %a.5 = alloca i64, i32 1
    %i = insert_value @X undef, i64 3, 2
    %a.7 = extract_value @X %i, 0
    store ptr %a.1, i64 %a.7
    %a.9 = extract_value @X %i, 1
    store ptr %a.3, i64 %a.9
    %a.11 = extract_value @X %i, 2
    store ptr %a.5, i64 %a.11
    branch i1 %cond, label %if, label %then

  %if:                        # preds: entry
    store ptr %a.1, i64 1
    store ptr %a.3, i64 2
    goto label %end

  %then:                      # preds: entry
    store ptr %a.1, i64 5
    store ptr %a.3, i64 6
    goto label %end

  %end:                       # preds: if, then
    %x = load i64, ptr %a.1
    %y = load i64, ptr %a.3
    %z = load i64, ptr %a.5
    %r.0 = add i64 %x, i64 %y
    %r.1 = add i64 %r.0, i64 %z
    return i64 %r.1
})");
}

TEST_CASE("SROA - 4", "[opt][sroa]") {
    test::passTest(&opt::sroa,
                   R"(
struct @X {
    i64, i64, i64
}
func i64 @main(i1 %cond) {
  %entry:
    %array = alloca @X, i32 3
    %x.1 = getelementptr inbounds @X, ptr %array, i32 1
    %x.1.value = load @X, ptr %x.1
    call void @takeX, @X %x.1.value
    %x.1.1 = getelementptr inbounds @X, ptr %x.1, i32 0, 1
    store ptr %x.1.1, i64 5
    %x.0.2 = getelementptr inbounds @X, ptr %array, i32 0, 2
    store ptr %x.0.2, i64 3
    goto label %end
    
  %end:
    %y.1.1 = getelementptr inbounds @X, ptr %x.1, i32 0, 1
    %a = load i64, ptr %y.1.1
    %y.0.2 = getelementptr inbounds @X, ptr %array, i32 0, 2
    %b = load i64, ptr %y.0.2
    %res = add i64 %a, i64 %b
    return i64 %b
}
func void @takeX(@X %0) {
  %entry:
    return
})",
                   R"(
struct @X {
  i64,
  i64,
  i64
}
func i64 @main(i1 %cond) {
  %entry:
    %array_2.0 = alloca i64, i32 1
    %array_0.2 = alloca i64, i32 1
    %array_1.2 = alloca i64, i32 1
    %array_2.2 = alloca i64, i32 1
    %x.1.value.1 = load i64, ptr %array_0.2
    %x.1.value.3 = insert_value @X undef, i64 %x.1.value.1, 0
    %x.1.value.5 = load i64, ptr %array_1.2
    %x.1.value.7 = insert_value @X %x.1.value.3, i64 %x.1.value.5, 1
    %x.1.value.9 = load i64, ptr %array_2.2
    %x.1.value.11 = insert_value @X %x.1.value.7, i64 %x.1.value.9, 2
    call void @takeX, @X %x.1.value.11
    store ptr %array_1.2, i64 5
    store ptr %array_2.0, i64 3
    goto label %end

  %end:                       # preds: entry
    %a = load i64, ptr %array_1.2
    %b = load i64, ptr %array_2.0
    %res = add i64 %a, i64 %b
    return i64 %b
}
func void @takeX(@X %0) {
  %entry:
    return
})");
}

TEST_CASE("SROA - 5", "[opt][sroa]") {
    test::passTest(&opt::sroa,
                   R"(
struct @X {
  i64, i64, i64
}
func @X @main(@X %0) {
  %entry:
    %x.0 = alloca @X, i32 1
    store ptr %x.0, @X %0
    %member.ptr = getelementptr inbounds @X, ptr %x.0, i64 0, 2
    store ptr %member.ptr, i64 1
    %tmp = load i64, ptr %member.ptr
    %x.1 = load @X, ptr %x.0
    return @X %x.1
})",
                   R"(
struct @X {
  i64, i64, i64
}
func @X @main(@X %0) {
  %entry:
    %x.0_0.0 = alloca i64, i32 1
    %x.0_1.0 = alloca i64, i32 1
    %x.0_2.0 = alloca i64, i32 1
    %x.0_0.2 = extract_value @X %0, 0
    store ptr %x.0_0.0, i64 %x.0_0.2
    %x.0_1.2 = extract_value @X %0, 1
    store ptr %x.0_1.0, i64 %x.0_1.2
    %x.0_2.2 = extract_value @X %0, 2
    store ptr %x.0_2.0, i64 %x.0_2.2
    store ptr %x.0_2.0, i64 1
    %x.3 = load i64, ptr %x.0_0.0
    %x.5 = insert_value @X undef, i64 %x.3, 0
    %x.7 = load i64, ptr %x.0_1.0
    %x.9 = insert_value @X %x.5, i64 %x.7, 1
    %x.11 = load i64, ptr %x.0_2.0
    %x.13 = insert_value @X %x.9, i64 %x.11, 2
    return @X %x.13
})");
}
