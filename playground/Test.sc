

//struct X {
//    var r: &mut int;
//}
//
//public fn main() -> int {
//    var x: X;
//    var i = 0;
//    x.r = &i;
//   // x.r = 1;
//  //  return i;
//}





struct X {
    var value: int;
}
fn getValue(x: &X) -> int {
     return x.value;
}
public fn main() -> int {
    var x: X;
    x.value = 42;
    let result = x.getValue;
    return result;
}





/**** Test cases:

// Value declaration; Returns 1
public fn main() -> int {
    let a = 1;
    return a;
}

// Value assignment; Return 4
public fn main() -> int {
    let a = 2;
    a *= 2;
    return a;
}

// Reference declaration; Returns 1
public fn main() -> int {
    let a = 1;
    let r = &a;
    return r;
}

// Array declaration; Returns 2
public fn main() -> int {
    let a = [1, 2];
    return a[1];
}

// Reference to array element; Returns 3
public fn main() -> int {
    var a = [1, 2];
    let r = &mut a[1];
    r = 3;
    return a[1];
}

// Reference reassignment; Returns 2
public fn main() -> int {
    let a = 1;
    let b = 2;
    let r = &a;
    r = &b;
    return r;
}


****/





// struct X {
//     fn sum(&this) -> int {
//         var res = 0;
//         for i = 0; i < this.r.count; ++i {
//             res += this.r[i];
//         }
//         return res;
//     }
//     var x: int;
//     var r: &[int];
// }
//
// public fn main() -> int {
//     let a = [1, 2, 3];
//     var x: X;
//     x.r = &a;
//     return x.sum();
// }
