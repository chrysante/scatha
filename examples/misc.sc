

fn print(n: int) {
    __builtin_puti64(n);
}

fn print(msg: &str) {
    __builtin_putstr(&msg);
}


fn f(n: s64) {}
fn f(n: u64) {}

public fn main() -> int {
    var i = 5;
    var j: u32 = 4;

    f(0);

    return i * j;
}
