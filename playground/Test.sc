/*
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
*/

fn main() -> int {
    return f(1);
}

fn f(n: int) -> int {
    return n > zero() ? pass(n) : zero();
}

fn pass(n: int) -> int { return n; }
fn zero() -> int { return 0; }
