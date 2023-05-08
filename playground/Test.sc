
fn print(msg: &[byte]) {
    __builtin_putstr(&msg);
    __builtin_putchar(10);
}

fn f(value: &int) -> int {
    print("Const!");
    return 1;
}
    
fn f(value: &mut int) -> int {
    print("Mutable!");
    return 2;
}

public fn main() {
    let i = 0;
    f(&i);
    i.f();
    i.f;
    var j = 1;
    f(&j);
    f(&mut j);
    j.f();
    j.f;
}
