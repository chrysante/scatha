import std;
use testframework;

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
