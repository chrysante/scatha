




struct X {
    var a: bool;
    var b: bool;
    var c: bool;
    var d: int;
}

fn mod(x: X) -> X {
    x.c = true;
    return x;
}

fn main() -> int {
    var x: X;
    x.d = 1;
    return mod(x).d;
}


//struct X {
//    var y: Y;
//    var i: int;
//}
//
//struct Y {
//    var j: s32;
//    var k: s32;
//}
//
//fn main(b: bool) -> int {
//    var x: X;
//    x.y.j = 1;
//    x.y.k = 2;
//    x.i = 3;
//    var y: X;
//    y.y.j = 4;
//    y.y.k = 5;
//    y.i = 6;
//    var z: &X = b ? x : y;
//    let w = x.y;
//    let q = x;
//    return z.y.j + w.k;
//}

