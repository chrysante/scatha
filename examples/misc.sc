

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
    var i = 0;

    var x: X;

    x.setRef(&mut i);

    x.assign(1);

"asdf";

    return i;
}
