
fn print(text: &str) {
    __builtin_putstr(text);
}

fn print(n: int) {
    __builtin_puti64(n);
}

struct X {
    fn new(&mut this) {
        print("Default construct X\n");
        this.n = 7;
    }
    
    fn new(&mut this, n: int) {
        print("Value construct X\n");
        this.n = n;
    }

    fn new(&mut this, rhs: &X) {
        print("Copy construct X\n");
    }
    
    fn delete(&mut this) {
        print("Delete X(n == ");
        print(this.n);
        print(")\n");
    }
    
    var n: int;
}

struct Z {
    fn new(&mut this) { this.n = 3; }
    var n: int;
}

struct Y {
    var n: int;
    var x: X;
    var z: Z;
}

fn passRef(ref: &X, value: int) -> &X {
    return ref;
}

fn main() {
    var data: [Y, 2];
}
