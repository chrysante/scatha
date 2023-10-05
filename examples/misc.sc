

fn print(text: &str) {
    __builtin_putstr(text);
    __builtin_putstr("\n");
}

fn main(args: &[*str]) {
    for i = 0; i < args.count; ++i {
        print(*args[i]);
    }
}