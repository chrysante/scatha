public fn main() -> int {
    return g(20);
}
fn g(n: int) -> int {
    var result = 0;
    var j = 0;
    while j < n {
        let i = j;
        ++j;
        if i % 2 == 0 {
       //    __builtin_putchar(10);
            continue;
        }
        result += i;
        print(i, result);
        if (i >= 10) {
            break;
        }
    }
    return result;
}

fn print(n: int, m: int) {
    __builtin_puti64(n);
    __builtin_putchar(59);
    __builtin_puti64(m);
    __builtin_putchar(10);
}
