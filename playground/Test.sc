
struct X {
    var i: int;
    var f: float;
    var b: bool;
}

public fn main(x: X) -> X {
    x.b = true;
    return x;
}
