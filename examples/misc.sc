

fn print(n: int) {
    __builtin_puti64(n);
}

fn print(msg: &[byte]) {
    __builtin_putstr(&msg);
}

struct X {
    fn setRef(&mut this, r: &mut int) {
        this.ref = &mut r;
    }

    fn assign(&this, n: int) {
        this.ref = n;
    }

    var ref: &mut int;
}

public fn main() -> int {
    var i = 5;
    var j: u32 = 4;

    return i * j;
}
