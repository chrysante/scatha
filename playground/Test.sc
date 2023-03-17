
struct X {
    var i: int;
    var f: float;
}

fn f(i: int) -> int {
    var j = i * 2;
    if i > 2 {
        ++j;
    }
    return j;
}

fn main() -> int {
    var i = -3;
    return f(-i);
}


/*
fn f(x: X, b: bool) -> X {
    if b {
        x.i = 0;
    }
    return x;
}
fn g(n: int) -> int {
    var result = 1;
    for i = 1; i <= n; ++i {
        if i % 2 == 0 {
            result *= i;
        }
    }
    if result % 4 == 0 {
        ++result;
    }
    return result;
}

struct X {
    var i: int;
    var y: Y;
}

struct Y {
    var i: int;
}

*/
