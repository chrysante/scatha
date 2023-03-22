fn main() -> int {
    let i = 1;
    let j = 5;
    let k = 6;
    var a = i * k + 4 - 6;
    if (a > 5 * j) {
        a -= 10;
    }
    else {
        ++a;
    }
    return a % 7 == 0 ? a + 1 : (a + i) * 3;
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
