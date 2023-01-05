struct X {
    var b: bool;
    var c: bool;
    var d: bool;
    var a: int;
}

fn main() -> bool {
    var x: X;
    x.d = true;
    return x.d;
}
