import std;
use testframework;

fn stringCopyCtor() {
    var s = std.String("Hello World");
    var t = s;
    check("Counts are the same", s.count() == t.count());
    check("s is not empty after copy", !s.empty());
    check("t is not empty after copy", !t.empty());
    check("s and t have the same contents", strcmp(s.data(), t.data()));
}

fn stringMoveCtor() {
    var s = std.String("Hello World");
    var t = move s;
    check("s is empty after move", s.empty());
    check("t is not empty after copy", !t.empty());
    check("t has the contents that s held before the move", strcmp("Hello World", t.data()));
}

fn stringClear() {
    var s = std.String("Hello World");
    s.clear();
    check("String shall be empty after calling clear", s.empty());
}

fn stringAppendChar() {
    var s = std.String("Hello ");
    s.append('W');
    s.append('o');
    s.append('r');
    s.append('l');
    s.append('d');
    check("Append char", strcmp("Hello World", s.data()));
}

fn stringAppendStringRef() {
    var s = std.String("Hello ");
    s.append("World");
    check("Append string ref", strcmp("Hello World", s.data()));
}

fn stringAppendString() {
    var s = std.String("Hello ");
    s.append(std.String("World"));
    check("Append string", strcmp("Hello World", s.data()));
}

fn stringInsertStringRef() {
    var s = std.String("Hellorld!");
    s.insert(5, " Wo");
    check("Insert string", strcmp("Hello World!", s.data()));
}

fn main() {
    stringCopyCtor();
    stringMoveCtor();
    stringAppendChar();
    stringAppendStringRef();
    stringAppendString();    
    stringInsertStringRef();
    __builtin_putstr("PASSED: String tests\n");
    return 0;
}
