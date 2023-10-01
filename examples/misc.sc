

fn main() {
    let data = [1, 2, 3, 4, 5];
    var acc: int;
    for i = 0; i < data.count; ++i {
        acc += data[i];
    }
    return acc;
}