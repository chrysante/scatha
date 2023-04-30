


public fn main() -> int {
    let arr = [1, 2, 3, 4, 5];
    return sum(&arr, 5);
}

fn sum(data: &[int], size: int) -> int {
    var acc = 0;
    for i = 0; i < size; ++i {
        acc += data[i];
    }
    return acc;
}




