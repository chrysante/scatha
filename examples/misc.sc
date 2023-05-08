




fn print(n: int) {
    __builtin_puti64(n);
    __builtin_putchar(10);
}

fn print(msg: &[byte]) {
    __builtin_putstr(&msg);
    __builtin_putchar(10);
}

struct X {
    fn setRef(&mut this, r: &mut int) {
        this.ref = &r;
    }

    fn assign(&this, n: int) {
        this.ref = n;
    }

    var ref: &mut int;
}

public fn main() -> int {
    var i = 0;

    var x: X;

    x.setRef(&i);

    x.assign(1);

    return i;
}