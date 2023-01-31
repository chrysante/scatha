fn main() -> int {
    let x = 0;
    {
        let x = 1;
        return x;
    }
    return x;
}
