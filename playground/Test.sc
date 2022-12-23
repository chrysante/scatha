
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

fn fac(n: int) -> int {
    return n <= 1 ? 1 : fac(n - 1);
}

fn testAND(n: int) -> bool {
    return n == 1 && n == 5;
}

fn testOR(n: int) -> bool {
    return n == 1 || n == 5;
}
