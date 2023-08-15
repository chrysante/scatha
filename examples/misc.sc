fn print(cond: bool) {
    __builtin_putstr(cond ? "true" : "false");
}

public fn main(cond: bool) {
    if cond {
        print(cond);
    }
}