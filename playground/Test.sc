public fn main() -> int {
    let a = [1, 2, 3];
    let b: &[int] = &a;
    let c = [1, 2];
    b = &c;
    return b.count;
}
