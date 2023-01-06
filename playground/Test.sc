struct X {
    var b: bool;
    var c: bool;
    var d: bool;
    var a: int;
}

fn makeX() -> X {
    var x: X;
    x.a = 5;
    x.b = false;
    x.c = true;
    x.d = false;
    return forward(x);
}

fn forward(x: X) -> X {
    return x;
}

fn main() -> int {
    if forward(forward(makeX())).c {
        return -1;
    }
    return -2;
}
