
fn main() -> int {
    var x: X;
    x.y.i = 2;
    return x.y.i;
}


struct X {
    var y: Y;
}


struct Y {
    var i: int;
    var f: float;
}
