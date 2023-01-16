
fn main() -> int {
    return fac2(8);
}

fn fac(n: int) -> int {
    var result = 1;
    for i = 2; i <= n; i += 1 {
        result *= i;
    }
    return result;
}

fn fac2(n: int) -> int {
    if n <= 1 {
        return 1;
    }
    else {
        return n * fac2(n - 1);
    }
}
