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
    var z: Y;
    y.x.anInteger = 4;
    
    z.x = y.x;
    
    return z.x.anInteger;
}

