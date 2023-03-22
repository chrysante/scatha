fn pow(base: float, exp: int) -> float {
    var result: float = 1.0;
    var i = 0;
    if (exp < 0) {
        base = 1.0 / base;
        exp = -exp;
    }
    while i < exp {
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

/*

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
