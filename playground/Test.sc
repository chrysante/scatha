
struct X {
    fn f(&this, n: int) {}
}

public fn main(n: int, m: int) -> int {
    let a: int = 1;
    let b = 2;
    
    var r = &a;
    
    r = &b;
    return r;
}
