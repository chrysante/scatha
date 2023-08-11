
fn print(n: int) {
    __builtin_puti64(n);
    __builtin_putchar('\n');
}

fn print(msg: &str) {
    __builtin_putstr(&msg);
}

fn A(n: int) {
    print(n);
    B(n);
}

fn B(n: int) {
    if (n > 0) {
        A(n - 1);
    }
}

public fn main() -> int {
    var i = 5;
    A(i);
    return 0;
}
