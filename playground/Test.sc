
fn print(msg: &str) {
    __builtin_putstr(&msg);
}

fn f(n: s64) -> int { print("Signed\n"); return n; }

public fn main() -> int {
    var i: u32 = 8;
    return f(i);
}
