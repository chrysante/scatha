
fn f(m: int, n: int) -> int {
    return n;
}

fn g(n: int) -> int {
    var a = 2 * n;
    var b = 3 * n;
    
    var f1 = f(0, a);
    var f2 = f(0, b);
    
    return 1 + f1 + f2;
}

fn main(n: int) -> int {
    return g(1);
}
