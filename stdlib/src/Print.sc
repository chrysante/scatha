
public fn print(text: *str) -> void {
    __builtin_putln(text);
}

public fn print(text: *unique str) -> void {
    print(text as *);
}

public fn print(text: &String) -> void {
    print(text.data());
}
