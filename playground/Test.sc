

fn isPrime(n: int) -> bool {
    var i = 2;
    while (i < n) {
        if (n % i == 0) { return false; }
        i += 1;
    }
    return true;
}

fn nthPrime(n: int) -> int {
    var i = 0;
    var p = 1;
    while (i < n) {
        p += 1;
        if (isPrime(p)) {
            i += 1;
        }
    }
    return p;
}

fn main() -> int {
    var i = 0;
    return nthPrime(5);
}

fn fac(n: int) -> int {
    return n <= 1 ? 1 : fac(n - 1);
}




















/*



fn testAND(n: int) -> bool {
    return n == 1 && n == 5;
}

fn testOR(n: int) -> bool {
    return n == 1 || n == 5;
}

fn nestedLoop(n: int) -> int {
    var i = 0;
    if true {
        while (i < n) {
            i += 1;
        }
        return 0;
    }
    else {
        while (i < 2 * n) {
            i += 1;
        }
        return i % 7;
    }
}

fn nestedLoop(n: int, x: int) -> int {
    var i = 0;
    var result = 1;
    while (i < n) {
        var j = 0;
        while (j < n) {
            result *= 2;
            j += 1;
        }
        i += 1;
    }
    return result;
}

*/
