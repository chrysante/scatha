
fn print(msg: &str) {
    __builtin_putstr(&msg);
}

public fn main() {
    print("Hello world!\n");
}
