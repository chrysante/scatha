import std;

fn check(msg: &str, cond: bool) -> bool {
    if cond { return true; }
    __builtin_putstr("Test failed: ");
    __builtin_putstr(msg);
    __builtin_putstr("\n");
    return false;
}

fn require(msg: &str, cond: bool) {
    if !check(msg, cond) {
        __builtin_trap();
    }
}

fn strcmp(a: &str, b: &str) -> bool {
    if a.count != b.count { return false; }
    for i = 0; i < a.count; ++i {
        if a[i] != b[i] { return false; }
    }
    return true;
}

fn stringCopyCtor() {
    var s = std.String("Hello World");
    var t = s;
    check("Counts are the same", s.count() == t.count());
    check("s is not empty after copy", !s.empty());
    check("t is not empty after copy", !t.empty());
    check("s and t have the same contents", strcmp(*s.data(), *t.data()));
}

fn stringMoveCtor() {
    var s = std.String("Hello World");
    var t = move s;
    check("s is empty after move", s.empty());
    check("t is not empty after copy", !t.empty());
    check("t has the contents that s held before the move", strcmp("Hello World", *t.data()));
}

fn stringClear() {
    var s = std.String("Hello World");
    s.clear();
    check("String shall be empty after calling clear", s.empty());
}

fn main() {
    stringCopyCtor();
    stringMoveCtor();
    __builtin_putstr("PASSED: String tests\n");
}
