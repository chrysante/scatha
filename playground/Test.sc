
public fn main() -> int {
    var arr = [1, 2, 3, 4];
    var r = &arr[1];
    r = 5;
    return arr[1];// + arr[2];
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
