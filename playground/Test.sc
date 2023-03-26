fn fac(n: int) -> int {
    return n <= 1 ? 1 : n * fac(n - 1);
}

fn main() -> int {
    let res = (fac(5) + 0) * 1;
    return res - res;
}
