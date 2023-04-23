

public fn main() -> int {
    var i = 3;
    f(&i);
    return i;
}

fn f(x: &mut int)  {
    x += 1;
}
