fn fib(n: int) -> int {
    if n < 3 {
        return 1;
    }
    return fib(n - 1) + fib(n - 2);
}
public fn main() -> int {
    return fib(7);
}





// KEEP THIS: The pipeline (memtoreg, simplifycfg) fails on this function. Fix later
// public fn main(n: int, cond: bool) -> int {
//     if cond {}
//     else {}
//     n += 10;
//     return n;
// }


// simplifycfg fails on this function
// func i64 @main-s64-bool(i64 %0, i1 %1) {
//   %entry:
//     branch i1 1, label %loop.landingpad.0, label %loop.exit.0
//
//   %loop.landingpad.0:         # preds: entry
//     %expr.result.4 = add i64 %0, i64 1
//     %expr.result.6 = mul i64 %expr.result.4, i64 %expr.result.4
//     goto label %loop.footer
//
//   %loop.footer:               # preds: loop.landingpad.0, loop.footer
//     branch i1 1, label %loop.footer, label %loop.exit.0
//
//   %loop.exit.0:               # preds: entry, loop.footer
//     %acc.addr.5 = phi i64 [label %entry : 0], [label %loop.footer : %expr.result.6]
//     return i64 %acc.addr.5
// }

