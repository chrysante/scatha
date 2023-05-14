
fn print(msg: &str) {
    __builtin_putstr(&msg);
}

fn f(n: s64) { print("Signed\n"); }
fn f(n: u64) { print("Unsigned\n"); }

public fn main() -> int {
    var i: u32 = 0;
    f(i);
}
