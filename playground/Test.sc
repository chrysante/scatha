fn main() -> int {
    let x = 0;
    if -1002.0 > 0.0 {
        x = 0;
    }
    else {
        x = 1;
    }
    // just to throw some more complexity at the compiler
    let y = 1 + 2 * 3 / 4 % 5 / 6;
    if x == 1 {
        return x;
    }
    else {
        return x + 100;
    }
}
/*
*/
/*
fn fact(n: int) -> int {
    var i = 0;
    var result = 1;
    while i < n {
        i += 1;
        result *= i;
    }
    return result;
}
fn main() -> int {
    return fact(4);
}

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

*/
