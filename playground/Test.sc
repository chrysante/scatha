fn pow(base: float, exp: int) -> float {
    var result: float = 1.0;
    var i = 0;
    if (exp < 0) {
        base = 1.0 / base;
        exp = -exp;
    }
    while (i < exp) {
        result *= base;
        i += 1;
    }
    return result;
}
fn main() -> bool {
    var result = true;
    result &= pow( 0.5,  3) == 0.125;
    result &= pow( 1.5,  3) == 1.5 * 2.25;
    result &= pow( 1.0, 10) == 1.0;
    result &= pow( 2.0, 10) == 1024.0;
    result &= pow( 2.0, -3) == 0.125;
    result &= pow(-2.0,  9) == -512.0;
    return result;
}










// fn isPrime(n: int) -> bool {
//     var i = 2;
//     while (i < n) {
//         if (n % i == 0) { return false; }
//         i += 1;
//     }
//     return true;
// }
//
// fn nthPrime(n: int) -> int {
//     var i = 0;
//     var p = 1;
//     while (i < n) {
//         p += 1;
//         if (isPrime(p)) {
//             i += 1;
//         }
//     }
//     return p;
// }
// fn testAND(n: int) -> bool {
//     return n == 1 && n == 5;
// }
// 
// fn testOR(n: int) -> bool {
//     return n == 1 || n == 5;
// }

// fn doubleLoop() {
//     while (true) {}
//     while (true) {}
//     return;
// }
//
// fn singleLoop(n: int) -> int {
//     var i = 0;
//     var result = 1;
//     while (i < n) {
//         result *= 2;
//         i += 1;
//     }
//     return result;
// }
//
// fn nestedLoop(n: int) -> int {
//     var i = 0;
//     var result = 1;
//     while (i < n) {
//         var j = 0;
//         while (j < n) {
//             result *= 2;
//             j += 1;
//         }
//         i += 1;
//     }
//     return result;
// }
//
// fn doublyNestedLoop(n: int) -> int {
//     var i = 0;
//     var result = 1;
//     while (i < n) {
//         var j = 0;
//         while (j < n) {
//             var k = 0;
//             while (k < n) {
//
//                 result *= 2;
//                 k += 1;
//             }
//             j += 1;
//         }
//         i += 1;
//     }
//     return result;
// }



