fn f(a: int, b: int) -> int {
    let aSave = a;
    while a != b {
        let t = b;
        b = a % b;
        a = t;
    }
    let x = a + aSave;
    var r = 1;
    for i = 1; i <= x; ++i {
        r *= i;
    }
    return r;
}
