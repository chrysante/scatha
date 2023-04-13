fn pow(x: float, n: int) -> float {
    var result = 1.0;
    for i = 0; i < n; ++i {
        result *= x;
    }
    return result;
}
fn main() -> int {
    let x = pow(1.61, int(5.5));
    return int(x);
}
