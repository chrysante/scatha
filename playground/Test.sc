struct X {
    var anInteger: int;
    var aFloat: float;
    var aSecondInt: int;
}

struct Y {
    var i: int;
    var x: X;
}

fn main() -> int {
    var y: Y;
    y.x.aSecondInt = 4;
    return y.x.aSecondInt;
}

