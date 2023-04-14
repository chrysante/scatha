


public fn main() -> int {
    h();
    return int(1.0);
}

fn g() {
   h();
}

fn h() {
    g();
}
