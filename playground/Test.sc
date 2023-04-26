
public fn main() {
    return some_long_undeclared_id;
}


//    public fn main() -> int {
//        var x = 0;
//        var r = &x;
//        r = 1;
//        var s = &r;
//        s += 2;
//        return x;
//    }

// public fn main() -> int {
//     var i = 0;
//     var j = &i;
//     j = 1;
//     return i;
// }


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
