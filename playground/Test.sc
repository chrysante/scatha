
fn main() -> int {
    return faculty(5, 4);
}

fn faculty(n: int) -> int {
    var i = 1;
    var result = 1;
    while i <= n {
        result *= i;
        i += 1;
    }
    return result;
}

fn faculty(n: int, m: int) -> int {
    return -1;
}
