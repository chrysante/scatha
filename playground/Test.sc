
fn print(msg: &str) {
    __builtin_putstr(&msg);
}

fn print(n: int) {
    __builtin_puti64(n);
}

fn print(n: double) {
    __builtin_putf64(n);
}

public fn main() -> int {
    for i: double = 0.0; i < 10.0; i += 1.0 {
        print(i);
        print("\n");
    }
}
