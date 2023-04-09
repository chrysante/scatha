
fn f(a: int, b: int) -> int {
    let aSave = a;
    while a != b {
        let t = b;
        b = a % b;
        a = t;
    }
    return a + aSave;
}

struct X {
    var i: int;
    var f0: float;
    var f1: float;
    // var f2: float;
    // var f3: float;
}

fn g(x: X) -> X {
    return x;
}

//func i64 @f33(i64 %0, i64 %1) {
//  %entry:
//    goto label %loop.header
//
//  %loop.header:               # preds: entry, loop.body
//    %a = phi i64 [label %entry : %0], [label %loop.body : %b]
//    %b = phi i64 [label %entry : %1], [label %loop.body : %srem.res]
//    %cmp.result = scmp neq i64 %a, i64 %b
//    branch i1 %cmp.result, label %loop.body, label %loop.end
//
//  %loop.body:                 # preds: loop.header
//    %srem.res = srem i64 %a, i64 %b
//    goto label %loop.header
//
//  %loop.end:                  # preds: loop.header
//    %add.res = add i64 %a, i64 %0
//    return i64 %add.res
//}
