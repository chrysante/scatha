fn pass(data: &[int]) -> &[int] { return &data; }
public fn main() -> int {
    let data = [1, 2, 3];
    let result = pass(&data)[1];
    return result;
}

//fn pow(base: double, exp: int) -> double {
//    var result: double = 1.0;
//    var i = 0;
//    if (exp < 0) {
//        base = 1.0 / base;
//        exp = -exp;
//    }
//    while i < exp {
//        result *= base;
//        i += 1;
//    }
//    return result;
//}
//public fn main() -> bool {
//    var result = true;
//    //result &= pow( 0.5,  3) == 0.125;
//    //result &= pow( 1.5,  3) == 1.5 * 2.25;
//    //result &= pow( 1.0, 10) == 1.0;
//    //result &= pow( 2.0, 10) == 1024.0;
//    result &= pow( 2.0, -3) == 0.125;
//    //result &= pow(-2.0,  9) == -512.0;
//    return result;
//}





//struct V {
//    var x: int;
//    var y: int;
//    var z: int;
//}
//
//public fn main() -> int {
//    var v: V;
//    v.x = 1;
//    v.y = 2;
//    v.z = 3;
//    return f(g(v));
//}
//
//
//fn f(v: V) -> int {
//    return v.x + v.y + v.z;
//}
//
//fn g(v: V) -> V {
//    v.x += 1;
//    v.y += 1;
//    v.z += 1;
//    return v;
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

