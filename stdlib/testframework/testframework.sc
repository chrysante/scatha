
public fn check(msg: &str, cond: bool) -> bool {
    if cond { return true; }
    __builtin_putstr("Test failed: ");
    __builtin_putstr(msg);
    __builtin_putstr("\n");
    return false;
}

public fn require(msg: &str, cond: bool) {
    if !check(msg, cond) {
        __builtin_trap();
    }
}

public fn strcmp(a: &str, b: &str) -> bool {
    if a.count != b.count { return false; }
    for i = 0; i < a.count; ++i {
        if a[i] != b[i] { return false; }
    }
    return true;
}
