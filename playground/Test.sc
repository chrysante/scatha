fn main() -> int {
    var acc = 1;
    for i = 1; i <= 5; ++i {
        acc *= i;
    }
    return acc;
}
