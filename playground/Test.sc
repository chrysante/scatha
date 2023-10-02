
fn print(text: &str) {
    __builtin_putstr(text);
}

struct X {
    fn new(&mut this) {
        print("Default construct X\n");
    }

    fn new(&mut this, rhs: &X) {
        print("Copy construct X\n");
    }
    
    fn delete(&mut this) {
        print("Delete X\n");
    }
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

fn main() {
    var data: [Y, 3];
    
    
    
}
