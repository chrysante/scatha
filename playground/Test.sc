


fn f(x: &[int]) -> int {
    return x.count;
}


public fn main() -> int {
    let a = [1, 2, 3];
    
    let b: &[int] = &a;

    return b.f;
}
