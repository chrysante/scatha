
fn main() -> int {
    return fac(6);
}

fn fac(n: int) -> int {
    return n == 0 ? 1 : n * fac(n - 1);
}
