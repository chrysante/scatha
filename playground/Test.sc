

fn fac(n: int) {
    if n == 0 {
        return 1;
    }
    return n * fac2(n - 1);
}

fn fac2(n: int) {
    if n == 0 {
        return 1;
    }
    return n * fac(n - 1);
}

fn main() -> int {
    return fac(5);
}

