

fn pow(x: float, n: int) -> float {
    var result = 1.0;
    for i = 0; i < n; ++i {
        result *= x;
    }
    return result;
}
public fn main() -> int {
    let x = pow(1.61, int(5.5));
    return int(x);
}

//fn main() -> int {
//    var i = 3;
//    // f(&i);
//    return i;
//}
//
//fn foo(x: &mut int)  {
//    x += 1;
//}
