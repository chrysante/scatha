
public fn print(text: *str) -> void {
    __builtin_putln(text);
}

public fn print(text: *mut str) -> void {
    __builtin_putln(text);
}

public fn print(text: *unique str) -> void {
    print(text as *);
}

public fn print(text: *unique mut str) -> void {
    print(text as *);
}

public fn print(text: &String) -> void {
    print(text.data());
}

public fn ioWrite(text: *str) -> void {
    __builtin_putstr(text);
}

public fn ioWrite(text: *mut str) -> void {
    __builtin_putstr(text);
}

public fn ioWrite(text: *unique str) -> void {
    ioWrite(text as *);
}

public fn ioWrite(text: *unique mut str) -> void {
    ioWrite(text as *);
}

public fn ioWrite(text: &String) -> void {
    ioWrite(text.data());
}
