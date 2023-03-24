


fn main() -> bool {
    var x: X;
    var y: Y;
    y.b = true;
    x.y = y;
    return x.y.b;
}


struct X {
    var y: Y;
}
struct Y {
    var c: bool;
    var b: bool;
    
}


/*
fn g(cond: bool) -> int {
    var x: X;
    x.i = 0;
    x.y.j = 0;
    if cond {
        x.i = 1;
        var z: Z;
        x.y.z = z;
    }
    else {
        var y: Y;
        y.j = 1;
        y.k = 2;
        x.y = y;
    }
    return x.i + x.y.j + x.y.k; 
}

struct X {
    var i: int;
    var y: Y;
}

struct Y {
    var j: int;
    var k: int;
    var z: Z;
}
struct Z {
    var a: float;
    var b: int;
}
*/



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
