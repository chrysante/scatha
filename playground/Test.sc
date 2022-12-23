
fn main() -> int {
    return faculty(5);
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

fn rfac(n: int) -> int {
    if (n <= 1) { return 1; }
    return n * rfac(n - 1);
}
