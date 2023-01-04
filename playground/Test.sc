

struct X {
    var anInteger: int;
    var aFloat: float;
    var aSecondInt: int;
}

struct Y {
    var x: X;
}

fn main() -> int {
    var y: Y;
    //var i: int;
    //i = 1;
    y.x.aFloat = 1.0;
    return 0;
}

