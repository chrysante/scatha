#include <Catch/Catch2.hpp>

#include "test/EndToEndTests/PassTesting.h"

using namespace scatha;

TEST_CASE("Member access", "[end-to-end][member-access]") {
    test::checkReturns(4, R"(
struct Y {
    var i: int;
    var x: X;
}
public fn main() -> int {
    var y: Y;
    y.x.aSecondInt = 4;
    return y.x.aSecondInt;
}
struct X {
    var anInteger: int;
    var aDouble: double;
    var aSecondInt: int;
})");
}

TEST_CASE("Bool member access", "[end-to-end][member-access]") {
    test::checkReturns(2, R"(
public fn main() -> int {
    var x: X;
    x.d = true;
    if x.d { return 2; }
    return 1;
}
struct X {
    var b: bool;
    var c: bool;
    var d: bool;
    var a: int;
})");
}

TEST_CASE("Return structs", "[end-to-end][member-access]") {
    test::checkReturns(2, R"(
struct X {
    var b: bool;
    var c: bool;
    var d: bool;
    var a: int;
}
fn makeX() -> X {
    var result: X;
    result.a = 1;
    result.b = false;
    result.c = true;
    result.d = false;
    return result;
}
public fn main() -> int {
    var x = makeX();
    if x.c { return 2; }
    return 1;
})");
}

TEST_CASE("Pass structs as arguments", "[end-to-end][member-access]") {
    test::checkReturns(5, R"(
struct X {
    var b: bool;
    var c: bool;
    var d: bool;
    var a: int;
}
fn getX_a(x: X) -> int {
    var result = x.a;
    return result;
}
public fn main() -> int {
    var x: X;
    x.a = 5;
    x.b = true;
    x.c = false;
    x.d = true;
    var result = getX_a(x);
    return result;
})");
}

TEST_CASE("Pass and return structs and access rvalue",
          "[end-to-end][member-access]") {
    test::checkReturns(5, R"(
public fn main() -> int {
    var x: X;
    x.a = 5;
    x.b = true;
    x.c = false;
    x.d = true;
    var y = forward(x);
    return y.a;
}
fn forward(x: X) -> X {
    return x;
}
struct X {
    var b: bool;
    var c: bool;
    var d: bool;
    var a: int;
})");
}

TEST_CASE("More complex structure passing", "[end-to-end][member-access]") {
    test::checkReturns(5, R"(
struct X {
    var b: bool;
    var c: bool;
    var d: bool;
    var a: int;
    var y: Y;
}
struct Y {
    var i: int;
    var f: double;
}
fn makeX() -> X {
    var x: X;
    x.a = 5;
    x.b = false;
    x.c = true;
    x.d = false;
    x.y = makeY();
    return forward(x);
}
fn makeY() -> Y {
    var y: Y;
    y.i = -1;
    y.f = 0.5;
    return y;
}
fn forward(x: X) -> X { return x; }
fn forward(y: Y) -> Y { return y; }
public fn main() -> int {
    if forward(makeX().y).f == 0.5 {
        return 5;
    }
    return 6;
})");
}

TEST_CASE("Member access mem2reg failure", "[end-to-end][member-access]") {
    test::checkReturns(1, R"(
fn modifyX(x: X) -> X {
    x.a = 1;
    return x;
}
public fn main() -> int {
    var x: X;
    x.a = 0;
    x.b = 0;
    x = modifyX(x);
    return x.a;
}
struct X {
    var a: int;
    var b: int;
})");
}

TEST_CASE("Array indexing", "[end-to-end][array-access]") {
    test::checkIRReturns(3, R"(
func i64 @main() {
  %entry:
    %a = alloca i64, i32 5
    call void @set, ptr %a, i32 2, i64 3
    %r = call i64 @get, ptr %a, i32 2
    return i64 %r
}
func i64 @get(ptr %data, i32 %index) {
  %entry:
    %e = getelementptr inbounds i64, ptr %data, i32 %index
    %r = load i64, ptr %e
    return i64 %r
}
func void @set(ptr %data, i32 %index, i64 %value) {
  %entry:
    %e = getelementptr inbounds i64, ptr %data, i32 %index
    store ptr %e, i64 %value
    return
})");
}

TEST_CASE("Nested array member access",
          "[end-to-end][member-access][array-access]") {
    test::checkIRReturns(2, R"(
struct @X {
    i64, i64
}
func i64 @main() {
  %entry:
    %a = alloca @X, i32 10
    %p = call ptr @populate, ptr %a
    %q = getelementptr inbounds @X, ptr %p, i32 0, 1
    %res = load i64, ptr %q
    return i64 %res
}
func ptr @populate(ptr %a) {
  %entry:
    %index = add i32 1, i32 2
    %p = getelementptr inbounds @X, ptr %a, i32 %index
    %x.0 = insert_value @X undef, i64 1, 0
    %x.1 = insert_value @X %x.0, i64 2, 1
    store ptr %p, @X %x.1
    return ptr %p
})");
}

TEST_CASE("Loop over array", "[end-to-end][array-access]") {
    test::checkIRReturns(55, R"(
func i32 @main() {
  %entry:
    %data = alloca i32, i32 10
    call void @fill, ptr %data, i32 10
    %res = call i32 @sum, ptr %data, i32 10
    return i32 %res
}
func void @fill(ptr %data, i32 %count) {
  %entry:
    %data-var = alloca ptr, i32 1
    store ptr %data-var, ptr %data
    goto label %header
    
  %header:
    %i = phi i32 [label %entry: 0], [label %body: %inc]
    %cond = ucmp ls i32 %i, i32 %count
    branch i1 %cond, label %body, label %end
    
  %body:
    %data.0 = load ptr, ptr %data-var
    %p = getelementptr inbounds i32, ptr %data.0, i32 %i
    %inc = add i32 %i, i32 1
    store ptr %p, i32 %inc
    goto label %header
  
  %end:
    return
}
func i32 @sum(ptr %data, i32 %count) {
  %entry:
    goto label %header
    
  %header:
    %i = phi i32 [label %entry: 0], [label %body: %inc]
    %acc = phi i32 [label %entry: 0], [label %body: %acc2]
    %cond = ucmp ls i32 %i, i32 %count
    branch i1 %cond, label %body, label %end
    
  %body:
    %inc = add i32 %i, i32 1
    %p = getelementptr inbounds i32, ptr %data, i32 %i
    %x = load i32, ptr %p
    %acc2 = add i32 %acc, i32 %x
    goto label %header
  
  %end:
    return i32 %acc
})");
}

TEST_CASE("Nested structure access of small types",
          "[end-to-end][array-access]") {
    test::checkIRReturns(7, R"(
struct @Y { i32 }
struct @X { @Y, i32 }
func i32 @main() {
  %entry:
    %0 = insert_value @X undef, i32 3, 0, 0
    %1 = insert_value @X %0, i32 4, 1
    %2 = extract_value @X %1, 0, 0
    %3 = extract_value @X %1, 1
    %4 = add i32 %2, i32 %3
    return i32 %4
})");
}
