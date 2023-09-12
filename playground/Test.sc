
fn f(n: int, m: int) -> int {
    return n + m;
}

fn g(n: int) -> int {
    var a = 1 + n;
    var b = 5 * n;
    
    var c = 2 + n;
    var d = 7 * n;
    
    var f1 = f(a, b);
    var f2 = f(c, d);
    
    return 1 + f1 + f2;
}

fn main(n: int) -> int {
    return g(1);
}
