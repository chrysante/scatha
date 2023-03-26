

fn fac(n: int) -> int {
    return n <= 1 ? 1 : fac(n - 1);
}

fn main() -> int {
    return fac(100);
}
