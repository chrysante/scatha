struct X {
    var b: bool;
    var c: bool;
    var d: bool;
    var a: int;
}

fn forward(x: X) -> X {
    return x;
}

fn main() -> int {
    var x: X;
    x.a = 5;
    // x.b = true;
    // x.c = false;
    // x.d = true;
    return forward(x).a;
}
