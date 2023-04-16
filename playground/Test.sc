
public fn main() {
    let n = 10;
    for i = 0; i < 10; ++i {
        if i % 2 == 0 || i == 5 {
            __builtin_puti64(i);
            __builtin_putchar(10);
        }
        else {
            __builtin_puti64(n);
            __builtin_putchar(10);
        }
    }
}
