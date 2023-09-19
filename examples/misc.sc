
fn print(text: &str) {
    __builtin_putstr(text);
}

struct X {
    fn new(&mut this) {

    }

    fn new(&mut this, n: int) {
        this.n = n;
    }

    fn new(&mut this, rhs: &X) {

    }

    fn delete(&mut this) {

    }

    var n: int;
}

fn main(cond: bool) -> int {
    var x = X();
    print("Hello world!\n");
    return 1;
}
