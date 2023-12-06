struct X {
    var i: int;
    var f: float;
    struct Y {
        var k: int;
        var b: byte;
    }
    var y: Y;
}
fn main() -> int {
    let x = X(2, 1.0, X.Y(1, 1));
    return /* x.i + int(x.f) + x.y.k +*/ int(x.y.b);
}




/*
/// Test case to assert that we don't match load-arithmetic if the load is an
/// execution dependency on the LHS operand
export fn test(p: *mut int) {
    let val = *p;
    *p = 1;
    return *p * val;
}
*/
/*
export fn test(n: s64) {
    var sum = 0;
    for i = 0; i < n; ++i {
        sum += i;
    }
    return sum;
}
*/
/*
export fn test(a: int, data:  *mut [byte], ints: *[int]) {
    __builtin_puti64(0);
    let localData = [data[4], '2', '3'];
    __builtin_puti64(1);
    let n = 2 * a;
    __builtin_memcpy(data, &localData);
    let index = ints[a];
    if (a + ints[a + 1]) / index + ints[1] == 0 {
        return n / 2;
    }
    return n;
}
*/
