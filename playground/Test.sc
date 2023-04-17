
struct X {
    var i: int;
    var j: int;
    var k: int;
}

public fn main(x: X) -> X {
    x.k = 1;
    return x;
}
