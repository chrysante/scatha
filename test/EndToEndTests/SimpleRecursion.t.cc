#include <Catch/Catch2.hpp>

#include <string>

#include "IR/CFG.h"
#include "IR/Module.h"
#include "Opt/TailRecElim.h"
#include "test/EndToEndTests/BasicCompiler.h"

using namespace scatha;

TEST_CASE("Recursive euclidean algorithm", "[end-to-end]") {
    test::checkReturns(7, R"(
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
    test::checkReturns(55, R"(
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
    test::checkReturns(3628800, R"(
fn main() -> int {
    return fact(10);
}
fn fact(n: int) -> int {
    if n <= 1 {
        return 1;
    }
    return n * fact(n - 1);
})");
    test::checkReturns(9223372036854775807ull, R"(
fn main() -> int { return fac(6); }
fn fac(n: int) -> int {
    return n <= 1 ? 1 : n | fac((n << 2) + 1);
})");
    test::checkReturns(2147483647, R"(
fn main() -> int { return fac(1459485138); }
fn fac(n: int) -> int {
    return n <= 2 ? 1 : n | fac((n >> 1) + 1);
}
fn pass(n: int) -> int { return n; }
)");
    test::checkReturns(1688818043, R"(
fn main() -> int { return fac(1459485138); }
fn fac(n: int) -> int {
    return n <= 2 ? 1 : n ^ fac((n >> 1) + 1);
})");
    test::checkReturns(0, R"(
fn main() -> int { return fac(1459485138); }
fn fac(n: int) -> int {
    return n <= 2 ? 1 : n & fac((n >> 1) + 1);
})");
    test::checkIRReturns(120,
                         R"(
func i64 @f(i64) {
  %entry:
    %cond = cmp leq i64 %0, i64 1
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
})",
                         [](ir::Context& ctx, ir::Module& mod) {
        for (auto& f: mod.functions()) {
            opt::tailRecElim(ctx, f);
        }
    });
}

TEST_CASE("Recursive pow", "[end-to-end]") {
    test::checkReturns(243, R"(
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
    test::checkIRReturns(1, R"(
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
    %cond = cmp leq i64 1, i64 undef
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
