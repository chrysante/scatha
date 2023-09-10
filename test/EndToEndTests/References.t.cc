#include <Catch/Catch2.hpp>

#include "test/EndToEndTests/PassTesting.h"

using namespace scatha;

TEST_CASE("First reference parameter", "[end-to-end][references]") {
    test::checkReturns(4, R"(
public fn main() -> int {
    var i = 3;
    f(i);
    return i;
}
fn f(x: &mut int)  {
    x += 1;
})");
}

TEST_CASE("Rebind pointer", "[end-to-end][references]") {
    test::checkReturns(2, R"(
public fn main() -> int {
    var i = 0;
    var j = 0;
    var r = &mut i;
    *r += 1;
    r = &mut j;
    *r += 1;
    return i + j;
})");
}

TEST_CASE("Pass reference through function", "[end-to-end][references]") {
    test::checkReturns(1, R"(
public fn main() -> int {
    var i = 0;
    var j: &mut int = f(i);
    j = 1;
    return i;
}
fn f(x: &mut int)  -> &mut int {
    return x;
})");
}

TEST_CASE("Pass array reference through function",
          "[end-to-end][references][arrays]") {
    test::checkReturns(2, R"(
fn pass(data: &[int]) -> &[int] { return data; }
public fn main() -> int {
    let data = [1, 2, 3];
    let result = pass(data)[1];
    return result;
})");
}

TEST_CASE("Pointer data member in struct", "[end-to-end][references]") {
    test::checkReturns(1, R"(
struct X {
    var i: *mut int;
}
public fn main() -> int {
    var i = 0;
    var x: X;
    x.i = &mut i;
    f(x);
    return i;
}
fn f(x: X)  {
    ++*x.i;
})");
}

TEST_CASE("First array", "[end-to-end][arrays]") {
    test::checkReturns(2, R"(
public fn main() -> int {
    var arr: [int, 4] = [1, 2, 3, 4];
    return arr[1];
})");
}

TEST_CASE("Reference to array element", "[end-to-end][arrays]") {
    test::checkReturns(5, R"(
public fn main() -> int {
    var arr = [1, 2, 3, 4];
    var r: &mut int = arr[1];
    r = 5;
    return arr[1];
})");
}

TEST_CASE("Use array elements", "[end-to-end][arrays]") {
    test::checkReturns(24, R"(
public fn main() -> int {
    var arr = [1, 2, 3, 4];
    return (arr[0] + arr[1] + arr[2]) * arr[3];
})");
}

TEST_CASE("Sum array with for loop", "[end-to-end][arrays]") {
    test::checkReturns(45, R"(
public fn main() -> int {
    let data = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9];
    var sum  = 0;
    for i = 0; i < 10; ++i {
        sum += data[i];
    }
    return sum;
})");
}

TEST_CASE("Array reference passing", "[end-to-end][arrays][references]") {
    test::checkReturns(2, R"(
public fn main() -> int {
    let x = [0, 1, 2, 3, 4];
    return getElem(x);
}
fn getElem(x: &[int]) -> int {
    return x[2];
})");

    test::checkReturns(2, R"(
public fn main() -> int {
    let x = [0, 1, 2, 3, 4];
    return getElem(x);
}

fn getElem(x: &[int]) -> &int {
    return x[2];
})");

    test::checkReturns(2, R"(
public fn main() -> int {
    let x = [0, 1, 2, 3, 4];
    let y = &x;
    return getElem(*y);
}

fn getElem(x: &[int]) -> &int {
    return x[2];
})");
}

TEST_CASE("Array `.count` member", "[end-to-end][arrays]") {
    test::checkReturns(2, R"(
public fn main() -> int {
    let x = [1, 2];
    return x.count;
})");

    test::checkReturns(5, R"(
public fn main() -> int {
    let x = [0, 1, 2, 3, 4];
    return getCount(x);
}
fn getCount(x: &[int]) -> int {
    return x.count;
})");

    test::checkReturns(7, R"(
public fn main() -> int {
    let x = [-3, 1, 2, 3, 4];
    return sum(x);
}
fn sum(x: &[int]) -> int {
    var s = 0;
    for i = 0; i < x.count; ++i {
        s += x[i];
    }
    return s;
})");
}

TEST_CASE("Reassign array pointer", "[end-to-end][arrays][references]") {
    test::checkReturns(2, R"(
public fn main() -> int {
    let a = [1, 2, 3];
    var b: *[int] = &a;
    let c = [1, 2];
    b = &c;
    return (*b).count;
})");
}

TEST_CASE("Array pointer struct member", "[end-to-end][arrays][references]") {
    test::checkReturns(4, R"(
struct X {
    fn sum(&this) -> int {
        return (*this.r)[0] + (*this.r)[1];
    }
    var x: int;
    var r: *mut [int];
}
public fn main() -> int {
    var a = [1, 2];
    var x: X;
    x.r = &mut a;
    ++(*x.r)[0];
    return x.sum;
})");
}

TEST_CASE("Copy array", "[end-to-end][arrays]") {
    test::checkReturns(1, R"(
public fn main() -> int {
    let a = [1, 2];
    let b = a;
    return b[0];
})");
}

TEST_CASE("Array of heterogeneous but compatible types",
          "[end-to-end][arrays]") {
    test::checkReturns(6, R"(
public fn main() -> int {
    let a = [u32(1), 2, s8(3)];
    var sum = 0;
    for i = 0; i < a.count; ++i {
        sum += a[i];
    }
    return sum;
})");
}

TEST_CASE("First string", "[end-to-end][arrays]") {
    test::checkPrints("Hello World!\n", R"(
fn print(text: &str) {
    __builtin_putstr(text);
    __builtin_putchar('\n');
}
public fn main() {
    print("Hello World!");
})");
}

TEST_CASE("Array slicing", "[end-to-end][arrays][references]") {
    test::checkReturns(3, R"(
public fn main() -> int {
    let data = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10];
    return data[2:5].count;
})");

    test::checkReturns(2, R"(
public fn main() -> int {
    let data = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10];
    return data[2:5][0];
})");

    test::checkReturns(15, R"(
fn sum(data: &[int]) -> int {
    var result = 0;
    for i = 0; i < data.count; ++i {
        result += data[i];
    }
    return result;
}
public fn main() -> int {
    let data = [5, 3, 1, 2, 3, 4, 5, 6, 100, -45213];
    return sum(data[2:7]);
})");
}

TEST_CASE("Return array by value", "[end-to-end][arrays]") {
    test::checkReturns(1, R"(
fn makeArray() -> [int, 10] {
    return [1, 2, 3, 4, 5, 6, 7, 8, 9, 10];
}
public fn main() -> int {
    return makeArray()[0];
})");
}

TEST_CASE("Dynamic allocation", "[end-to-end][arrays]") {
    test::checkReturns(45, R"(
public fn main() -> int {
    var data = allocateInts(10);
    for i = 0; i < (*data).count; ++i {
        (*data)[i] = i;
    }
    var sum = 0;
        for i = 0; i < (*data).count; ++i {
        sum += (*data)[i];
    }
    deallocateInts(data);
    return sum;
}
fn allocateInts(count: int) -> *mut [int] {
    var result = __builtin_alloc(count * 8, 8);
    return reinterpret<*mut [int]>(result);
}
fn deallocateInts(data: *mut [int]) {
    let bytes = reinterpret<*mut [byte]>(data);
    __builtin_dealloc(bytes, 8);
})");
}
