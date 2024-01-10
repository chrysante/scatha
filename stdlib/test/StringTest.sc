use std.String;

fn main() {
    var s = String("Hello world");
    __builtin_putstr(s.data());
    __builtin_putstr("\n");
}
