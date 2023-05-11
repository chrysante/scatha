

struct X {
    struct Y  {
        var i: int;
    }
    var k: int;

}


//public fn main() {
//    var x: X;
//    var y: x.Y;
//}

//public fn main() {
//    &reinterpret;
//}

//public fn main()->int { return 0; }

fn clamp(x: double, min: double, max: double) -> double {
    return x < min ? min : x > max ? max : x;
}

public fn printDot(x: double) {
    let data = [' ', '.', ':', '-', '=', '+', '*', '#', '%', '@'];
    let c = data[int(10.0 * clamp(x, 0.0, 1.0))];
    __builtin_putchar(c);
}
