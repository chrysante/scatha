fn pass(data: &[int]) -> &[int] {
    return data;
}

public fn main() -> int {
    let data = [1, 2, 3];
    let result = pass(data)[1];
    return result;
}
