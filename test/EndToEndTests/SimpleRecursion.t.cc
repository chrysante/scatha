#include <catch2/catch_test_macros.hpp>

#include <string>

#include "EndToEndTests/PassTesting.h"
#include "IR/CFG.h"
#include "IR/Module.h"
#include "Opt/Passes.h"

using namespace scatha;

TEST_CASE("Recursive euclidean algorithm", "[end-to-end]") {
    test::runReturnsTest(7, R"(
fn main() -> int {
    let a = 756476;
    let b = 1253;
    return gcd(a, b);
}
fn gcd(a: int, b: int) -> int {
    if (b == 0) {
        return a;
    }
    return gcd(b, a % b);
})");
}

TEST_CASE("Recursive fibonacci", "[end-to-end]") {
    test::runReturnsTest(55, R"(
fn main() -> int {
    let n = 10;
    return fib(n);
}
fn fib(n: int) -> int {
    if (n == 0) {
        return 0;
    }
    if (n == 1) {
        return 1;
    }
    return fib(n - 1) + fib(n - 2);
})");
}

TEST_CASE("Recursive factorial and weird variations", "[end-to-end]") {
    test::runReturnsTest(3628800, R"(
fn main() -> int {
    return fact(10);
}
fn fact(n: int) -> int {
    if n <= 1 {
        return 1;
    }
    return n * fact(n - 1);
})");
    test::runReturnsTest(9223372036854775807ull, R"(
fn main() -> int { return fac(6); }
fn fac(n: int) -> int {
    return n <= 1 ? 1 : n | fac((n << 2) + 1);
})");
    test::runReturnsTest(2147483647, R"(
fn main() -> int { return fac(1459485138); }
fn fac(n: int) -> int {
    return n <= 2 ? 1 : n | fac((n >> 1) + 1);
}
fn pass(n: int) -> int { return n; }
)");
    test::runReturnsTest(1688818043, R"(
fn main() -> int { return fac(1459485138); }
fn fac(n: int) -> int {
    return n <= 2 ? 1 : n ^ fac((n >> 1) + 1);
})");
    test::runReturnsTest(0, R"(
fn main() -> int { return fac(1459485138); }
fn fac(n: int) -> int {
    return n <= 2 ? 1 : n & fac((n >> 1) + 1);
})");
    test::runIRReturnsTest(120,
                           R"(
func i64 @f(i64) {
  %entry:
    %cond = scmp leq i64 %0, i64 1
    branch i1 %cond, label %then, label %else
  
  %then:
    return i64 1

  %else:
    %sub.res = sub i64 %0, i64 1
    %call.res = call i64 @f, i64 %sub.res
    %mul.res = mul i64 %0, i64 %call.res
    return i64 %mul.res
}

func i64 @main() {
  %entry:
    %call.res = call i64 @f, i64 5
    return i64 %call.res
})");
}

TEST_CASE("Recursive pow", "[end-to-end]") {
    test::runReturnsTest(243, R"(
fn main() -> int {
     return pow(3, 5);
}
fn pow(base: int, exponent: int) -> int {
    if exponent == 0 {
        return 1;
    }
    if exponent == 1 {
        return base;
    }
    if exponent % 2 == 0 {
        return pow(base *  base, exponent / 2);
    }
    return base * pow(base *  base, exponent / 2);
})");
}

TEST_CASE("Direct tail recursion", "[end-to-end]") {
    test::runIRReturnsTest(1, R"(
struct @X {
  i64,
  i64
}

func i64 @main() {
  %entry:
    %q = alloca @X
    %p = getelementptr inbounds @X, ptr %q, i64 0, 0
    store ptr %p, i64 1
    %i = load i64, ptr %p
    goto label %loopheader

  %loopheader:                # preds: entry
    %cond = scmp leq i64 1, i64 undef
    goto label %loopbody

  %loopbody:                  # preds: loopheader, loopbody
    %x = phi ptr [label %loopheader : %p], [label %loopbody : undef]
    branch i1 1, label %exit, label %loopbody

  %exit:                      # preds: loopbody
    %res = call i64 @get_value, ptr undef, ptr undef, i1 1
    %s = select i1 1, f64 undef, f64 undef
    return i64 1
}

func i64 @get_value(ptr %0, ptr %1, i1 %2) {
  %entry:
    branch i1 %2, label %then, label %else

  %then:                      # preds: entry
    %cond = lnt i1 %2
    %res.0 = call i64 @get_value, ptr %1, ptr %0, i1 %cond
    goto label %ret

  %else:                      # preds: entry
    goto label %ret

  %ret:                       # preds: then, else
    %res = phi i64 [label %then : %res.0], [label %else : 1]
    return i64 %res
})");
}

TEST_CASE("Mutual recursion", "[end-to-end]") {
    test::runReturnsTest(56, R"(
fn f(n: int) -> int {
    return n * 2;
}
fn r1(n: int) -> int {
    if n > 0 {
        return r2(n);
    }
    return f(n);
}
fn r2(n: int) -> int {
    return n + r1(n - 1);
}
fn main() -> int {
    return r1(1) + r1(10);
})");
}

TEST_CASE("Ackermann function", "[end-to-end]") {
    test::runReturnsTest(125, R"(
fn ack(n: int, m: int) -> int {
    if n == 0 {
        return m + 1;
    }
    if m == 0 {
        return ack(n - 1, 1);
    }
    return ack(n - 1, ack(n, m - 1));
}
fn main() -> int {
    return ack(3, 4);
})");
}

TEST_CASE("One level of recursion", "[end-to-end]") {
    test::runReturnsTest(3, R"(
struct Expr {
    var id: int;
    var lhs: int;
    var rhs: int;
}
fn eval(expr: mut Expr) -> int {
    // 0 == Literal
    if expr.id == 0 {
        return expr.lhs;
    }
    // 1 == Add
    if expr.id == 1 {
        return expr.lhs + expr.rhs;
    }
    // 2 == Sub
    expr.id = 1; // Add
    expr.rhs = -expr.rhs;
    return eval(expr);
}
fn main() -> int {
    var expr: Expr;
    expr.id = 2;
    expr.lhs = 5;
    expr.rhs = 2;
    return eval(expr);
})");
}

TEST_CASE("Swap", "[end-to-end]") {
    test::runReturnsTest(1, R"(
fn main() -> bool {
    var a = 1;
    var b = 2;
    var c = 3;
    swap(a, b); // a = 2, b = 1
    swap(b, c); // b = 3, c = 1
    return a == 2 && b == 3 && c == 1;
}
fn swap(a: &mut int, b: &mut int) {
    let tmp = a;
    a = b;
    b = tmp;
})");
}

TEST_CASE("Sort", "[end-to-end]") {
    test::runReturnsTest(1, R"(
fn main() -> bool {
    var data = [
        15,   50,   82,   57,    7,   42,   86,   23,   60,   51,
        17,   19,   80,   33,   49,   35,   79,   98,   89,   27,
        92,   45,   43,    5,   87,    2,   58,   75,   22,   18,
        30,   41,   70,   40,    3,   84,   63,   39,   56,   97,
        81,    6,   64,   47,   90,   20,   77,   12,   74,   55,
        78,    4,   28,   52,   61,   85,   32,   37,   95,   83,
        87,   54,   77,   72,    9,   65,   11,   31,   10,    1,
        25,   73,   44,   71,   68,    8,   67,   13,   91,   24,
        62,   21,   66,   48,   99,   94,   69,   46,  100,   38,
        16,   53,   96,   34,   59,   14,   29,   93,   36,   26
    ];
    sort(data);
    return isSorted(data);
}

fn sort(data: &mut [int]) -> void {
    if data.empty {
        return;
    }
    let p = partition(data);
    sort(data[0 : p]);
    sort(data[p + 1 : data.count]);
}

fn partition(data: &mut [int]) -> int {
    var i = 0;
    var j = data.count - 2;
    let pivot = data.back;
    while i < j {
        while i < j && data[i] <= pivot {
            ++i;
        }
        while j > i && data[j] > pivot {
            --j;
        }
        if data[i] > data[j] {
            swap(data[i], data[j]);
        }
    }
    if data[i] <= pivot {
        return data.count - 1; // index of pivot
    }
    swap(data[i], data.back);
    return i;
}

fn swap(a: &mut int, b: &mut int) {
    let tmp = a;
    a = b;
    b = tmp;
}

fn isSorted(data: &[int]) -> bool {
    for i = 0; i < data.count - 1; ++i {
        if data[i] > data[i + 1] {
            return false;
        }
    }
    return true;
})");
}

TEST_CASE("Sort tree", "[end-to-end]") {
    test::runReturnsTest(1, R"(
struct Node {
    fn new(&mut this, n: int) {
        this.value = n;
    }
    
    var value: int;
    var left: *unique mut Node;
    var right: *unique mut Node;
}

fn insert(root: mut *unique mut Node,
          newNode: mut *unique mut Node) {
    var y: *mut Node = null;
    var x: *mut Node = root;
    while x != null {
        y = x;
        if newNode.value < x.value {
            x = x.left;
        }
        else {
            x = x.right;
        }
     }
     if y == null {
        return move newNode;
     }
     else if newNode.value < y.value {
        y.left = move newNode;
        return move root;
     }
     else {
        y.right = move newNode;
        return move root;
     }
}

fn extractResult(root: *Node, data: &mut [int]) {
    var index = 0;
    extractResultImpl(root, data, index);
}

fn extractResultImpl(node: *Node, data: &mut [int], index: &mut int) -> void {
    if node == null {
        return;
    }
    extractResultImpl(node.left, data, index);
    data[index++] = node.value;
    extractResultImpl(node.right, data, index);
}

fn isSorted(data: &[int]) {
    for i = 0; i < data.count - 1; ++i {
        if data[i] > data[i + 1] {
            return false;
        }
    }
    return true;
}

fn main() {
    var root: *unique mut Node = null;
    let data = [
        23, 35, 73,  4, 92, 14, 27,  96, 22, 48,
        46, 25, 87, 76, 21, 77, 28,  37, 18, 82,
        36, 65, 39,  2, 69, 86, 59,  91, 95, 43,
        34,  9, 42, 58,  6, 74, 19,  93, 41, 26,
        12, 50, 53, 72, 75, 45, 54,   3,  5, 89,
         7, 79, 24, 97, 44, 66, 57,  64, 68, 83,
        47, 63, 61, 99, 38, 20, 52,  67, 85, 71,
        49, 70, 29, 84, 31,  8, 88,  94, 16, 78,
        60, 32, 62, 30, 17, 80, 90,  55, 13, 33,
        15, 51, 40, 81, 98,  1, 11, 100, 10, 56
    ];
    for i = 0; i < data.count; ++i {
        root = insert(move root, unique Node(data[i]));
    }
    var result: [int, data.count];
    extractResult(root, result);
    return isSorted(result);
})");
}
