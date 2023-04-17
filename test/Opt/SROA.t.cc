#include <Catch/Catch2.hpp>

#include "Opt/SROA.h"
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
    %a = alloca @X, i32 10
    %p.0 = getelementptr inbounds @X, ptr %a, i32 3
    %x.0 = insert_value @X undef, i64 1, 0
    %x.1 = insert_value @X %x.0, i64 2, 1
    store ptr %p.0, @X %x.1
    %q.0 = getelementptr inbounds @X, ptr %p.0, i32 0, 1
    %p.1 = getelementptr inbounds @X, ptr %a, i32 5
    %x.2 = insert_value @X undef, i64 1, 0
    %x.3 = insert_value @X %x.0, i64 2, 1
    store ptr %p.1, @X %x.3
    %q.1 = getelementptr inbounds @X, ptr %p.1, i32 0, 0
    %res.0 = load i64, ptr %q.0
    %res.1 = load i64, ptr %q.1
    %res = add i64 %res.0, i64 %res.1
    return i64 %res
})",
                   R"(
struct @X {
  i64, i64
}
func i64 @main() {
  %entry:
    %a.0 = alloca @X, i32 1
    %a.1 = alloca i64, i32 1
    %a.2 = alloca i64, i32 1
    %x.0 = insert_value @X undef, i64 1, 0
    %x.1 = insert_value @X %x.0, i64 2, 1
    %a.1.val = extract_value @X %x.1, 1
    store ptr %a.1, i64 %a.1.val
    %x.2 = insert_value @X undef, i64 1, 0
    %x.3 = insert_value @X %x.0, i64 2, 1
    %a.2.val = extract_value @X %x.3, 0
    store ptr %a.2, i64 %a.2.val
    %res.0 = load i64, ptr %a.1
    %res.1 = load i64, ptr %a.2
    %res = add i64 %res.0, i64 %res.1
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
    %x.slice_1.0 = alloca i64, i32 1
    %x.slice_1.2 = extract_value @X %0, 1
    store ptr %x.slice_1.0, i64 %x.slice_1.2
    %x.1.value = load i64, ptr %x.slice_1.0
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
  i64, i64, i64
}

func i64 @main(i1 %cond) {
  %entry:
    %a.slice.0 = alloca @X, i32 1
    %a.slice_0.0 = alloca i64, i32 1
    %a.slice_1.0 = alloca i64, i32 1
    %a.slice_2.0 = alloca i64, i32 1
    %i = insert_value @X undef, i64 3, 2
    %a.slice_0.2 = extract_value @X %i, 0
    store ptr %a.slice_0.0, i64 %a.slice_0.2
    %a.slice_1.2 = extract_value @X %i, 1
    store ptr %a.slice_1.0, i64 %a.slice_1.2
    %a.slice_2.2 = extract_value @X %i, 2
    store ptr %a.slice_2.0, i64 %a.slice_2.2
    branch i1 %cond, label %if, label %then

  %if:                        # preds: entry
    store ptr %a.slice_0.0, i64 1
    store ptr %a.slice_1.0, i64 2
    goto label %end

  %then:                      # preds: entry
    store ptr %a.slice_0.0, i64 5
    store ptr %a.slice_1.0, i64 6
    goto label %end

  %end:                       # preds: if, then
    %x = load i64, ptr %a.slice_0.0
    %y = load i64, ptr %a.slice_1.0
    %z = load i64, ptr %a.slice_2.0
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
    i64, i64, i64
}
func i64 @main(i1 %cond) {
  %entry:
    %array.slice_2.0 = alloca i64, i32 1
    %array.slice_1.0 = alloca i64, i32 1
    %x.1.value.slice.0 = load i64, ptr %array.slice_1.0
    %x.1.value.slice.2 = insert_value @X undef, i64 %x.1.value.slice.0, 1
    call void @takeX, @X %x.1.value.slice.2
    store ptr %array.slice_1.0, i64 5
    store ptr %array.slice_2.0, i64 3
    goto label %end

  %end:                       # preds: entry
    %a = load i64, ptr %array.slice_1.0
    %b = load i64, ptr %array.slice_2.0
    %res = add i64 %a, i64 %b
    return i64 %b
}
func void @takeX(@X %0) {
  %entry:
    return
})");
}
