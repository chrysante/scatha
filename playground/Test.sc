
struct X {
    var i: &mut int;
}
public fn main() -> int {
    var i = 0;
    var x: X;
    x.i = &i;
    f(x);
    return i;
}
fn f(x: X)  {
    ++x.i;
}

