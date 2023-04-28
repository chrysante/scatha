//public fn main() -> int {
//    var i = 3;
//    f(& &i);
//    return i;
//}
//
//fn f(x: &mut int)  {
//    x += 1;
//}


public fn main() -> int {
    var i = 0;
    var j = 0;
    var r = &i;
    r += 1;
    r = &j;
    r += 1;
    return i + j;
}


/*

struct X {
    var i: &mut int;
}


public fn main() -> int {
    var x: X;
    var i = 3;
    
    /// Assign the reference member `i`
    x.i = &i;
    
    f(x);
    return i;
}

fn f(x: X)  {
    /// Assign through the reference
    x.i += 1;
}

*/
