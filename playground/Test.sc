

struct X {
    var i: int;
    var f: float;
    struct Y {
        var k: int;
        var b: byte;
    }
    var y: Y;
}

fn main() -> int {
    let x = X(2, 1.0, X.Y(1, 'X'));
    return x.i + int(x.f);
}
