#include <catch2/catch_test_macros.hpp>

#include "IR/CFG/BasicBlock.h"
#include "IR/CFG/Function.h"
#include "IR/Context.h"
#include "IR/IRParser.h"
#include "IR/Module.h"
#include "Opt/PassTest.h"
#include "Opt/Passes.h"

using namespace scatha;
using namespace opt;
using namespace ir;

TEST_CASE("SROA - 1", "[opt][sroa]") {
    test::passTest(opt::sroa,
                   R"(
struct @X {
  i64, i64
}
func i64 @main() {
  %entry:
    %data = alloca @X, i32 10
    
    // Make variable x
    %x.tmp = insert_value @X undef, i64 1, 0
    %x = insert_value @X %x.tmp, i64 2, 1
    
    // Make variable y
    %y.tmp = insert_value @X undef, i64 1, 0
    %y = insert_value @X %y.tmp, i64 2, 1
    
    // Store x into array index 3
    %data.at.3 = getelementptr inbounds @X, ptr %data, i32 3
    store ptr %data.at.3, @X %x
    
    // Store y into array index 5
    %data.at.5 = getelementptr inbounds @X, ptr %data, i32 5
    store ptr %data.at.5, @X %y
    
    // Load second data member from index 3
    %member.1 = getelementptr inbounds @X, ptr %data.at.3, i32 0, 1
    %lhs = load i64, ptr %member.1
    
    // Load first data member from index 5
    %member.0 = getelementptr inbounds @X, ptr %data.at.5, i32 0, 0
    %rhs = load i64, ptr %member.0
    
    // Sum lhs and rhs
    %res = add i64 %lhs, i64 %rhs
    
    return i64 %res
})",
                   R"(
struct @X {
  i64, i64
}
func i64 @main() {
  %entry:
    %x.tmp = insert_value @X undef, i64 1, 0
    %x = insert_value @X %x.tmp, i64 2, 1
    %y.tmp = insert_value @X undef, i64 1, 0
    %y = insert_value @X %y.tmp, i64 2, 1
    %sroa.extract.0 = extract_value @X %x, 0
    %sroa.extract.2 = extract_value @X %x, 1
    %sroa.extract.4 = extract_value @X %y, 0
    %sroa.extract.6 = extract_value @X %y, 1
    %res = add i64 %sroa.extract.2, i64 %sroa.extract.4
    return i64 %res
})");
}

TEST_CASE("SROA - 2", "[opt][sroa]") {
    test::passTest(opt::sroa,
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
    %sroa.extract.0 = extract_value @X %0, 0
    %sroa.extract.2 = extract_value @X %0, 1
    return i64 %sroa.extract.2
})");
}

TEST_CASE("SROA - 3", "[opt][sroa]") {
    test::passTest(opt::sroa,
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
    %p.1 = getelementptr inbounds @X, ptr %a, i32 3   // Same as p.0
    %p.2 = getelementptr inbounds @X, ptr %p.1, i32 0, 0
    store ptr %p.2, i64 1
    %p.3 = getelementptr inbounds @X, ptr %p.1, i32 0, 1
    store ptr %p.3, i64 2
    goto label %end

  %then:
    %p.4 = getelementptr inbounds @X, ptr %a, i32 3   // Same as p.0
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
    %i = insert_value @X undef, i64 3, 2
    %sroa.extract.0 = extract_value @X %i, 0
    %sroa.extract.2 = extract_value @X %i, 1
    %sroa.extract.4 = extract_value @X %i, 2
    branch i1 %cond, label %if, label %then

  %if:                        // preds: entry
    goto label %end

  %then:                      // preds: entry
    goto label %end

  %end:                       // preds: if, then
    %a.slice.6 = phi i64 [label %if : 2], [label %then : 6]
    %a.slice.5 = phi i64 [label %if : 1], [label %then : 5]
    %r.0 = add i64 %a.slice.5, i64 %a.slice.6
    %r.1 = add i64 %r.0, i64 %sroa.extract.4
    return i64 %r.1
})");
}

TEST_CASE("SROA - 4", "[opt][sroa]") {
    test::passTest(opt::sroa,
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
    %sroa.insert.0 = insert_value @X undef, i64 undef, 0
    %sroa.insert.2 = insert_value @X %sroa.insert.0, i64 undef, 1
    %sroa.insert.4 = insert_value @X %sroa.insert.2, i64 undef, 2
    call void @takeX, @X %sroa.insert.4
    goto label %end

  %end:                       // preds: entry
    %res = add i64 5, i64 3
    return i64 3
}
func void @takeX(@X %0) {
  %entry:
    return
})");
}

TEST_CASE("SROA - Override one struct member", "[opt][sroa]") {
    test::passTest(opt::sroa,
                   R"(
struct @X {
    i64, i64, i64
}
func @X @main(@X %0) {
  %entry:
    %data = alloca @X, i32 1
    store ptr %data, @X %0
    %member.ptr = getelementptr inbounds @X, ptr %data, i64 0, 2
    store ptr %member.ptr, i64 1
    %result = load @X, ptr %data
    return @X %result
})",
                   R"(
struct @X {
    i64, i64, i64
}
func @X @main(@X %0) {
  %entry:
    %sroa.extract.0 = extract_value @X %0, 0
    %sroa.extract.2 = extract_value @X %0, 1
    %sroa.extract.4 = extract_value @X %0, 2
    %sroa.insert.0 = insert_value @X undef, i64 %sroa.extract.0, 0
    %sroa.insert.2 = insert_value @X %sroa.insert.0, i64 %sroa.extract.2, 1
    %sroa.insert.4 = insert_value @X %sroa.insert.2, i64 1, 2
    return @X %sroa.insert.4
})");
}

TEST_CASE("SROA - Store array, load element", "[opt][sroa]") {
    test::passTest(opt::sroa,
                   R"(
func i64 @main([i64, 2] %0) {
  %entry:
    %data = alloca [i64, 2], i32 1
    store ptr %data, [i64, 2] %0
    %elem.ptr = getelementptr inbounds i64, ptr %data, i64 1
    %result = load i64, ptr %elem.ptr
    return i64 %result
})",
                   R"(
func i64 @main([i64, 2] %0) {
  %entry:
    %0.1 = extract_value [i64, 2] %0, 0
    %0.3 = extract_value [i64, 2] %0, 1
    return i64 %0.3
})");
}

TEST_CASE("SROA - Store array, load nested element", "[opt][sroa]") {
    test::passTest(opt::sroa,
                   R"(
struct @X { i32, i32 }
func i32 @main([@X, 2] %0) {
  %entry:
    %data = alloca [@X, 2], i32 1
    store ptr %data, [@X, 2] %0
    %elem.ptr = getelementptr inbounds @X, ptr %data, i64 1, 0
    %result = load i32, ptr %elem.ptr
    return i32 %result
})",
                   R"(
struct @X { i32, i32 }
func i32 @main([@X, 2] %0) {
  %entry:
    %sroa.extract.0 = extract_value [@X, 2] %0, 0
    %sroa.extract.2 = extract_value [@X, 2] %0, 1, 0
    %sroa.extract.4 = extract_value [@X, 2] %0, 1, 1
    return i32 %sroa.extract.2
})");
}

TEST_CASE("SROA - Store elements, load array", "[opt][sroa]") {
    test::passTest(opt::sroa,
                   R"(
func [i64, 2] @main(i64 %0, i64 %1) {
  %entry:
    %data = alloca [i64, 2], i32 1
    %at.0 = getelementptr inbounds i64, ptr %data, i64 0
    store ptr %at.0, i64 %1
    %at.1 = getelementptr inbounds i64, ptr %data, i64 1
    store ptr %at.1, i64 %0
    %result = load [i64, 2], ptr %data
    return [i64, 2] %result
})",
                   R"(
func [i64, 2] @main(i64 %0, i64 %1) {
  %entry:
    %sroa.insert.0 = insert_value [i64, 2] undef, i64 %1, 0
    %sroa.insert.2 = insert_value [i64, 2] %sroa.insert.0, i64 %0, 1
    return [i64, 2] %sroa.insert.2
})");
}

TEST_CASE("SROA - Store nested elements, load array", "[opt][sroa]") {
    test::passTest(opt::sroa,
                   R"(
struct @X {
    i64, i64
}
func [@X, 2] @main(@X %0, i64 %1, i64 %2) {
  %entry:
    %data = alloca [@X, 2]
    %at.0 = getelementptr inbounds @X, ptr %data, i64 0
    store ptr %at.0, @X %0
    %at.1.0 = getelementptr inbounds @X, ptr %data, i64 1, 0
    store ptr %at.1.0, i64 %1
    %at.1.1 = getelementptr inbounds @X, ptr %data, i64 1, 1
    store ptr %at.1.1, i64 %2
    %result = load [@X, 2], ptr %data
    return [@X, 2] %result
})",
                   R"(
struct @X {
    i64, i64
}
func [@X, 2] @main(@X %0, i64 %1, i64 %2) {
  %entry:
    %sroa.insert.0 = insert_value [@X, 2] undef, @X %0, 0
    %sroa.insert.2 = insert_value [@X, 2] %sroa.insert.0, i64 %1, 1, 0
    %sroa.insert.4 = insert_value [@X, 2] %sroa.insert.2, i64 %2, 1, 1
    return [@X, 2] %sroa.insert.4
})");
}

TEST_CASE("SROA - Access nodes generated from store", "[opt][sroa]") {
    test::passTest(opt::sroa,
                   R"(
func i64 @main([i64, 2] %0) {
  %entry:
    %data = alloca [i64, 2], i32 1
    store ptr %data, [i64, 2] %0
    %result = load i64, ptr %data
    return i64 %result
})",
                   R"(
func i64 @main([i64, 2] %0) {
  %entry:
    %sroa.extract.0 = extract_value [i64, 2] %0, 0
    %sroa.extract.2 = extract_value [i64, 2] %0, 1
    return i64 %sroa.extract.0
})");
}

TEST_CASE("SROA - Phi instruction with only one argument", "[opt][sroa]") {
    test::passTest(opt::sroa,
                   R"(
func i32 @main() {
%entry:
    %local = alloca i32, i32 1
    store ptr %local, i32 1
    branch i1 1, label %then, label %else
    
%then:
    %p = phi ptr [label %entry: %local]
    goto label %cond.end

%else:
    goto label %cond.end

%cond.end:
    %q = phi ptr [label %then: %p], [label %else: %local]
    %res = load i32, ptr %q
    return i32 %res
})",
                   R"(
func i32 @main() {
  %entry:
    branch i1 1, label %then, label %else

  %then:                      // preds: entry
    goto label %cond.end

  %else:                      // preds: entry
    goto label %cond.end

  %cond.end:                  // preds: then, else
    %res.phi.0 = phi i32 [label %then : 1], [label %else : 1]
    return i32 %res.phi.0
})");
}

TEST_CASE("SROA - Phi alloca pointer with opaque pointer", "[opt][sroa]") {
    test::passTest(opt::sroa,
                   R"(
func i64 @test(i1 %0, ptr %1) {
%entry:
    %local = alloca i64, i32 1
    branch i1 %0, label %then, label %else
    
%then:
    goto label %if.end

%else:
    goto label %if.end

%if.end:
    %p = phi ptr [label %then: %1], [label %else: %local]
    store ptr %p, i64 1
    goto label %end

%end:
    %ret = load i64, ptr %p
    return i64 %ret
})",
                   R"(
func i64 @test(i1 %0, ptr %1) {
  %entry:
    branch i1 %0, label %then, label %else

  %then:                      // preds: entry
    store ptr %1, i64 1
    %ret.1 = load i64, ptr %1
    goto label %if.end

  %else:                      // preds: entry
    goto label %if.end

  %if.end:                    // preds: then, else
    %ret.phi.0 = phi i64 [label %then : %ret.1], [label %else : 1]
    goto label %end

  %end:                       // preds: if.end
    return i64 %ret.phi.0
})");
}

TEST_CASE("SROA memcpy after phi", "[opt][sroa]") {
    /// This is promotable but used to fail due to not handling call
    /// instructions in phi rewrite
    auto [ctx, mod] = ir::parse(R"(
ext func void @__builtin_memcpy(ptr, i64, ptr, i64)

func void @test() {
%entry:
    %data = alloca i64, i32 2
    %target = alloca i64, i32 1
    %elem = getelementptr inbounds i64, ptr %data, i32 1
    branch i1 1, label %foo, label %bar

%foo:
    goto label %bar

%bar:
    %merged = phi ptr [label %entry: %elem], [label %foo: %elem]
    call void @__builtin_memcpy, ptr %target, i64 8, ptr %merged, i64 8
})")
                          .value();
    auto& F = mod.front();
    bool modified = sroa(ctx, F);
    CHECK(modified);
    for (auto& BB: F) {
        CHECK(BB.emptyExceptTerminator());
    }
}

TEST_CASE("SROA memcpy argument defined after phi", "[opt][sroa]") {
    /// This is not promotable because we don't want to rewrite the other
    /// arguments to memcpy
    auto [ctx, mod] = ir::parse(R"(
ext func void @__builtin_memcpy(ptr, i64, ptr, i64)
ext func { ptr, i64 } @__builtin_alloc(i64, i64)

func void @test() {
%entry:
    %data = alloca i64, i32 2
    %elem = getelementptr inbounds i64, ptr %data, i32 1
    branch i1 1, label %foo, label %bar

%foo:
    goto label %bar

%bar:
    %merged = phi ptr [label %entry: %data], [label %foo: %elem]
    %alloc = call { ptr, i64 } @__builtin_alloc, i64 8, i64 8
    %alloc.ptr = extract_value { ptr, i64 } %alloc, 0
    call void @__builtin_memcpy, ptr %alloc.ptr, i64 8, ptr %merged, i64 8
})")
                          .value();
    auto& F = mod.front();
    bool modified = sroa(ctx, F);
    CHECK(!modified);
}

TEST_CASE("SROA store argument defined after phi", "[opt][sroa]") {
    /// This is not promotable because we don't want to rewrite the other
    /// operand of the store
    auto [ctx, mod] = ir::parse(R"(
ext func void @__builtin_memcpy(ptr, i64, ptr, i64)

func void @test(i64 %0) {
%entry:
    %data = alloca i64, i32 2
    %elem = getelementptr inbounds i64, ptr %data, i32 1
    branch i1 1, label %foo, label %bar

%foo:
    goto label %bar

%bar:
    %merged = phi ptr [label %entry: %data], [label %foo: %elem]
    %value = mul i64 %0, i64 2
    store ptr %merged, i64 %value
})")
                          .value();
    auto& F = mod.front();
    bool modified = sroa(ctx, F);
    CHECK(!modified);
}

TEST_CASE("SROA memcpy within alloca region", "[opt][sroa]") {
    test::passTest(opt::sroa,
                   R"(
struct @X { i64, i64 }
ext func void @__builtin_memcpy(ptr %0, i64 %1, ptr %2, i64 %3)
func i64 @main() {
  %entry:
    %addr = alloca @X, i32 1
    %0 = getelementptr inbounds @X, ptr %addr, i64 0, 0
    %1 = getelementptr inbounds @X, ptr %addr, i64 0, 1
    store ptr %0, i64 1
    store ptr %1, i64 2
    call void @__builtin_memcpy, ptr %0, i64 8, ptr %1, i64 8
    %r = load i64, ptr %0
    return i64 %r
})",
                   R"(
struct @X { i64, i64 }
ext func void @__builtin_memcpy(ptr %0, i64 %1, ptr %2, i64 %3)
func i64 @main() {
  %entry:
    return i64 2
})");
}

TEST_CASE("SROA load struct as double", "[opt][sroa]") {
    test::passTest(opt::sroa,
                   R"(
struct @X { i32, i16, i16 }
func f64 @f() {
  %entry:
    %addr = alloca @X, i32 1
    %0 = getelementptr inbounds @X, ptr %addr, i64 0, 0
    %1 = getelementptr inbounds @X, ptr %addr, i64 0, 1
    %2 = getelementptr inbounds @X, ptr %addr, i64 0, 2
    store ptr %0, i32 3
    store ptr %1, i16 1
    store ptr %2, i16 2
    %r = load f64, ptr %addr
    return f64 %r
})",
                   R"(
struct @X { i32, i16, i16 }
func f64 @f() {
  %entry:
    %sroa.zext = zext i32 3 to i64
    %sroa.or = or i64 0, i64 %sroa.zext
    %sroa.zext.0 = zext i16 1 to i64
    %sroa.shift = lshl i64 %sroa.zext.0, i32 32
    %sroa.or.0 = or i64 %sroa.or, i64 %sroa.shift
    %sroa.zext.1 = zext i16 2 to i64
    %sroa.shift.0 = lshl i64 %sroa.zext.1, i32 48
    %sroa.or.1 = or i64 %sroa.or.0, i64 %sroa.shift.0
    %sroa.bitcast = bitcast i64 %sroa.or.1 to f64
    return f64 %sroa.bitcast
})");
}

TEST_CASE("SROA store double to struct", "[opt][sroa]") {
    test::passTest(opt::sroa,
                   R"(
struct @X { i32, i32 }
func i32 @f() {
  %entry:
    %addr = alloca @X, i32 1
    %0 = getelementptr inbounds @X, ptr %addr, i64 0, 0
    %1 = getelementptr inbounds @X, ptr %addr, i64 0, 1
    %r = store ptr %addr, f64 1.234
    %r0 = load i32, ptr %0
    %r1 = load i32, ptr %1
    %r = and i32 %r0, i32 %r1
    return i32 %r
})",
                   R"(
struct @X { i32, i32 }
func i32 @f() {
  %entry:
    %sroa.bitcast = bitcast f64 1.234000 to i64
    %sroa.trunc = trunc i64 %sroa.bitcast to i32
    %sroa.shift = lshr i64 %sroa.bitcast, i32 32
    %sroa.trunc.0 = trunc i64 %sroa.shift to i32
    %r = and i32 %sroa.trunc, i32 %sroa.trunc.0
    return i32 %r
})");
}
