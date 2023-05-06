
fn print(text: &[byte]) {
    __builtin_putstr(&text);
    __builtin_putchar(10);
}

public fn main() {
    let a = [1, 2, 3];
    print("Hello World!");
}


