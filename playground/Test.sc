
struct V {
    var x: int;
    var y: int;
}

fn main() -> int {
    return test(4);
}

fn makeV(i: int) -> V {
    var v: V;
    v.x = 1;
    v.y = fac(i);
    return v;
}

fn test(i: int) -> int {
    var v = makeV(i);
    return v.y;
}

fn fac(n: int) -> int {
    var result = 1;
    for i = 2; i <= n; i += 1 {
        result *= i;
    }
    return result;
}
