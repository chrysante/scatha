


fn deref(a: &[int]) -> int { return a[0]; }


public fn main() -> int {
    let a = [1, 2, 3];
    // let b = &a;
    // return deref(&b);
    
    
    /// Still fails!
    return a.count;
}
