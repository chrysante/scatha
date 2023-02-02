
fn makeX() -> X { var x: X; return x; }

fn f() -> int {
    let x = makeX();
    return x.y.i;
}

struct X {
    var i: int;
    var y: Y;
}

struct Y {
    var i: int;
}
