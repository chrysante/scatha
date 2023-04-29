
public fn main() -> int {
    var acc = 0;
    for i = 0; i < 5; ++i {
        var z: Complex;
        z.x = 0;
        z.y = i;
        acc += sum(z);
    }
    return acc;
}
fn sum(z: Complex) -> int { return z.x + z.y; }
struct Complex {
    var x: int;
    var y: int;
}
