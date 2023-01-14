fn fact(n: int) -> int {
    var result = 1;
    for i = 1; i <= n; i += 1 {
        result *= i;
    }
    return result;
}
fn main() -> int {
    return fact(4);
}

/*

fn gcd(a: int, b: int) -> int {
    while a != b {
        a == b;
        if a > b {
            if true {
                a -= b;
            }
            else {}
        }
        else {
            if true {
                b -= a;
            }
            
        }
    }
    return a;
}
fn main() -> int {
    let a = 756476;
    let b = 1253;
    return gcd(a, b);
}

*/
