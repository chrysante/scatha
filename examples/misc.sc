
fn print(n: int) {
    __builtin_puti64(n);
    __builtin_putchar('\n');
}

fn print(msg: &str) {
    __builtin_putstr(&msg);
}

public fn main(cond: bool) -> int {
    var n = 0;
    if cond {
        n += 10;
    }
    else {
        n += 10;
    } 
    for i = 0; i < n; ++i {
        n -= 1;
    }
    return 0;
}
