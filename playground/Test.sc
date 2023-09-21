


struct X {
    var i: int;
}

fn f(x: X) -> X { return x; }

fn main() -> int {
    return f(X()).i;
}
