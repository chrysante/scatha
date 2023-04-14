


fn main() {
    var i = 1;
    print(i);
    i = cppCallback(i);
    print(i);
}

fn print(n: int) {
    __builtin_puti64(n);
    __builtin_putchar(10);
}

fn fac(n: int) -> int {
    return n <= 1 ? 1 : n * fac(n - 1);
}

fn callback(n: int) {
    print(n);
    print(fac(n));
}
