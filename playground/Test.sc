
fn main() -> int {
    let i = 1 + 20;
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


