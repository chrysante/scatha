
struct X {
    fn new(&mut this) {}
    fn new(&mut this, rhs: &X) {}
}

fn test(x: X, n: int) {}

fn main() {
    var x = X();
    var n = 0;
    test(x, n);
}
