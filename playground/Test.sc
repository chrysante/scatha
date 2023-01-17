
fn test() -> int {
    var i = 1;
    var j = 2;
    var k: int;
    if i + j < 4 {
        k = i;
    }
    else {
        k = j;
    }
    return k;
}

fn gcd(a: int, b: int) -> int {
    while b != 0 || true {
        let t = b;
        b = a % b;
        a = t;
    }
    return a;
}
