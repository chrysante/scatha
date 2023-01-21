fn main() -> int {
    var acc = 0;
    for k = 0; k < 2; k += 1 {
        for j = 0; j < 3; j += 1 {
            var i = 0;
            do {
                acc += 1;
                i += 1;
            } while i < 4;
        }
    }
    return acc;
}
