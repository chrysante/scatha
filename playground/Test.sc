    
fn gcd(a: int, b: int) -> int {
    while a != b {
        if a > b {
            a -= b;
        }
        else {
            b -= a;
        }
    }
    return a;
}
fn main() -> int {
    let a = 756476;
    let b = 1253;
    return gcd(a, b);
}
