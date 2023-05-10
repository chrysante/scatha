
fn print(msg: &[byte]) {
    __builtin_putstr(&msg);
}

public fn main() {
    print("Hello\t\nwor\\\ld\m\n" );
}
