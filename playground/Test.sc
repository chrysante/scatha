struct X {
    var b: bool;
    var c: bool;
    var d: bool;
    var a: int;
    var y: Y;
}
struct Y {
    var i: int;
    var f: float;
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
fn main() -> int {
    if forward(makeX().y).f == 0.5 {
        return 5;
    }
    return 6;
}





/*

#//fn f(a: int, b: int) -> int {
#//    let aSave = a;
#//    while a != b {
#//        let t = b;
#//        b = a % b;
#//        a = t;
#//    }
#//    return a + aSave;
#//}
#//
#//struct Y {
#//    var x: X;
#//}
#//
#//struct X {
#//    var i: int;
#//    var f0: float;
#//}
#//
#//fn g(y: Y) -> X {
#//    y.x.f0 = 1.0;
#//    return y.x;
#//}

#//func i64 @f33(i64 %0, i64 %1) {
#//  %entry:
#//    goto label %loop.header
#//
#//  %loop.header:               # preds: entry, loop.body
#//    %a = phi i64 [label %entry : %0], [label %loop.body : %b]
#//    %b = phi i64 [label %entry : %1], [label %loop.body : %srem.res]
#//    %cmp.result = scmp neq i64 %a, i64 %b
#//    branch i1 %cmp.result, label %loop.body, label %loop.end
#//
#//  %loop.body:                 # preds: loop.header
#//    %srem.res = srem i64 %a, i64 %b
#//    goto label %loop.header
#//
#//  %loop.end:                  # preds: loop.header
#//    %add.res = add i64 %a, i64 %0
#//    return i64 %add.res
#//}
#//
#//
#
#
#//struct @X {
#//  i64,
#//  f64,
#//  i8,
#//  i8
#//}
#//
#//func @X @g(@X %0, i8 %1) {
#//  %entry:
#//    %x.0 = extract_value @X %0, 0
#//    %x.1 = insert_value @X undef, i64 %x.0, 0
#//    %x.2 = insert_value @X %x.1, f64 1.000000, 1
#//    %x.3 = insert_value @X %x.2, i8 1, 2
#//    %x.4 = insert_value @X %x.3, i8 %1, 3
#//    return @X %x.4
#//}

*/
