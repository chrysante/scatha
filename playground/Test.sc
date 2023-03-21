/*
/// SCC 0
fn main() {
    f();
    i();
    z();
}

/// SCC 1
fn f() { g(); }

fn g() { z(); h(); }

fn h() { f(); }

/// SCC 2
fn i() { j(true); }

fn j(cond: bool) {
    if cond {
        z();
    }
    else {
        k();
    }
}

fn k() { i(); }

/// SCC 3
fn z() {  }
*/

fn main() -> int {
    let i = 20;
    return f(i);
}

fn f(n: int) -> int {
    var i = 0;
    while i < n {
        ++i;
        if i > 10 {
            return -1;
        }
    }
    return i;
}
