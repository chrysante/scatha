
fn f(m: int) {
    return g(m + 1);
}

fn g(n: int) {
    if n > 10 {
        return n;
    }
    return f(n);
}

fn main() {
    return f(0);
}
