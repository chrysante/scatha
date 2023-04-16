public fn main() -> int {
    var acc = 0;
    for j = 0; j < 2; ++j {
        for i = 0; i < 3; ++i {
            ++acc;
        }
    }
    return acc;
}
