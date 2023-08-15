
struct X {
    var i: int;
    var k: int;
    
    fn set(this) {
        this.i = 3;
        this.k = 7;
    }
}





public fn main() -> int {
    
    __builtin_putstr("Hello World!\n");
    
    
    
    
}




//public fn main() -> int {
//    var data = [5, 7, 2, 4];
//    var data2: &[int] = &data;
//    return sum(&data);
//}
//
//fn sum(data: &[int]) -> int {
//    var sum = 0;
//    for i = 0; i < data.count; ++i {
//        sum += data[i];
//    }
//    return sum;
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

