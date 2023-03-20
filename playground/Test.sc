

fn main() -> int {
    return f(4);
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
