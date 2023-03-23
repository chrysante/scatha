fn gcd(a: int, b: int) -> int {
    while b != 0 && true
     {
        let t = b + 0;
        b = a % b;
        a = t;
    }
    return a;
}
fn main() -> int {
    let a = 756476;
    let b = 1253;
    return gcd(a, b);// + gcd(1, 7);
}

/*
fn main() -> int { return f(1); }

fn f(n: int) -> int {
    var result = 0;
    if n == 0 {
        result = 10;
    }
    else {
        result = 42;
    }
    return result;
}


fn A(n: int) {
    B(n);
}

fn B(n: int) {
    C(n);
}

fn C(n: int) {
    D(n);
}

fn D(n: int) {
    E(n);
}

fn E(n: int) {
    F(n);
}

fn F(n: int) {
    if n == 0 {
        return;
    }
    else {
        A(n - 1);
    }
}

fn main() -> int {
    A(0);
    return 0;
}


fn A() { B(); }
fn B() { C(); D(); }
fn C() { A(); }
fn D() { E(); X(); X(); X(); }
fn E() { F(); G(); }
fn F() { D(); }
fn G() { H(); }
fn H() { I(); I2(); }
fn I() { G(); }
fn I2() { G(); }

fn X() { Y(); }
fn Y() {  }



fn main() -> int {
    return f(1);
}

fn f(n: int) -> int {
    return n > zero() ? pass(n) : zero();
}

fn pass(n: int) -> int { return n; }

fn zero() -> int { return 0; }

fn g(n: int) -> int {
    return n > 0 ? n : 0;
}
*/
