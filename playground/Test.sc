struct X {
    var b: bool;
    var c: bool;
    var d: bool;
    var a: int;
}
fn makeX() -> X {
    var result: X;
    result.a = 1;
    result.b = false;
    result.c = true;
    result.d = false;
    return result;
}
fn main() -> int {
    var x = makeX();
    if x.c { return 2; }
    return 1;
}

/*
struct V {
    var x: int;
    var y: int;
}

fn main() -> int {
    var v: V;
    v.x = 1;
    v.y = 2;
    return v.y;
}
*/
