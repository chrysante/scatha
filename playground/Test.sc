struct X {
    var b: bool;
    var c: bool;
    var d: bool;
    var a: int;
}
fn getX_a(x: X) -> bool {
    var result = x.a;
    return result;
}
fn main() -> int {
    var x;
    x.a = 5;
    x.b = true;
    x.c = false;
    x.d = true;
    var result = getX_a(x);
    return result;
}
