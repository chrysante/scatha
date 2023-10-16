#include <Catch/Catch2.hpp>

#include "test/EndToEndTests/PassTesting.h"

using namespace scatha;

TEST_CASE("Static data - 1", "[end-to-end][static-data]") {
    test::checkIRReturns(7, R"(
@const_data = constant [i32, 3] [i32 1, i32 2, i32 3]

@other_data = global i32 1

func i32 @main() {
  %entry:
    %1 = load i32, ptr @other_data
    %p = getelementptr inbounds i32, ptr @const_data, i32 0
    %t0 = load i32, ptr %p
    %q = getelementptr inbounds i32, ptr @const_data, i32 1
    %t1 = load i32, ptr %q
    store ptr @other_data, i32 3
    %t2 = load i32, ptr @other_data
    %s0 = add i32 %t0, i32 %t1
    %s1 = add i32 %s0, i32 %t2
    %s2 = add i32 %s1, i32 %1
    return i32 %s2
})");
}

TEST_CASE("Static data - 2", "[end-to-end][static-data]") {
    test::checkIRReturns(6, R"(
ext func void @__builtin_memcpy(ptr, i64, ptr, i64)

@global.data = constant [i32, 3] [i32 1, i32 2, i32 3]

func i32 @main() {
  %entry:
    %data = alloca i32, i32 3
    call void @__builtin_memcpy, ptr %data, i64 12, ptr @global.data, i64 12
    goto label %header
    
  %header:
    %i = phi i32 [label %entry : 0], [label %body : %i.1]
    %s = phi i32 [label %entry : 0], [label %body : %s.1]
    %c = ucmp ls i32 %i, i32 3
    branch i1 %c, label %body, label %end
    
  %body:
    %i.1 = add i32 %i, i32 1
    %p = getelementptr inbounds i32, ptr %data, i32 %i
    %elem = load i32, ptr %p
    %s.1 = add i32 %s, i32 %elem
    goto label %header
  
  %end:
    return i32 %s
})");
}

TEST_CASE("Static data - 3", "[end-to-end][static-data]") {
    test::checkIRPrints("Hello World!", R"(
@my_global = global [i8, 12] "Cello World!" # sic!

ext func void @__builtin_putstr(ptr, i64)

func i32 @main() {
%entry:
    store ptr @my_global, i8 72 # 72 == 'H'
    call void @__builtin_putstr, ptr @my_global, i64 12
    return i32 0
})");
}
