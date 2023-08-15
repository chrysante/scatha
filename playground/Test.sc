
public fn main(n: int) -> int {
    if n > 0 && n < 0 {
        return 1;
    }
    return 0;
}


//fn print(cond: bool) {
//    __builtin_putstr(cond ? "true" : "false");
//}
//
//public fn main(cond: bool) {
//    if cond {
//        print(cond);
//    }
//}









//# simplifycfg fails on this
//
//func i64 @main-s64-bool(i64 %0, i1 %1) {
//%entry:
//    branch i1 1, label %loop.landingpad.0, label %loop.exit.0
//
//%loop.landingpad.0:         # preds: entry
//    %expr.result.4 = add i64 %0, i64 1
//    %expr.result.6 = mul i64 %expr.result.4, i64 %expr.result.4
//    goto label %loop.footer
//
//%loop.footer:               # preds: loop.landingpad.0, loop.footer
//    branch i1 1, label %loop.footer, label %loop.exit.0
//
//%loop.exit.0:               # preds: entry, loop.footer
//    %acc.addr.5 = phi i64 [label %entry : 0], [label %loop.footer : %expr.result.6]
//    return i64 %acc.addr.5
//}

