
fn print(text: &[byte]) {
    __builtin_putstr(&text);
    __builtin_putchar(10);
}

public fn main() {
    print("Hello World!");
}


