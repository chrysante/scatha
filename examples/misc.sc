
struct X {
    var n: int;
}

fn main() {
    var x = X(1);
    x.n += 2;
    return x.n;
}