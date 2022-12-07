
fn main() -> int {
    return fakulaet(4);
}

fn fakultaet(n: int) -> int {
    var i = 1;
    var result = 1;
    while (i <= n) {
        result = result * i;
        i = i + 1;
    }
    return result;
}
