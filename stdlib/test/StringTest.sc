import std;

fn CHECK(msg: &str, cond: bool) -> bool {
    if cond { return true; }
    __builtin_putstr("Test failed: ");
    __builtin_putstr(msg);
    __builtin_putstr("\n");
    return false;
}

fn REQUIRE(msg: &str, cond: bool) {
    if !CHECK(msg, cond) {
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
    CHECK("Counts are the same", s.count() == t.count());
    CHECK("s is not empty after copy", !s.empty());
    CHECK("t is not empty after copy", !t.empty());
    CHECK("s and t have the same contents", strcmp(s.data(), t.data()));
}

fn stringMoveCtor() {
    var s = std.String("Hello World");
    var t = move s;
    CHECK("s is empty after move", s.empty());
    CHECK("t is not empty after copy", !t.empty());
    CHECK("t has the contents that s held before the move", strcmp("Hello World", t.data()));
}

fn main() {
    stringCopyCtor();
    stringMoveCtor();
    __builtin_putstr("All string tests run\n");
}
