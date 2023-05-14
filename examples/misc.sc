
fn print(n: int) {
    __builtin_puti64(n);
}

fn print(msg: &str) {
    __builtin_putstr(&msg);
}

fn f(n: s64) { print("Signed\n"); }

fn f(n: u64) { print("Unsigned\n"); }

public fn main() -> int {
    var i = 5;
    var u: u32 = 4;

    f(i); // Prints "Signed"
    f(u); // Ambiguous call

    return i * u;
}
