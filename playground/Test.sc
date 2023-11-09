
fn sum(data: &[int]) -> int {
    return data.empty ? 0 : data.front + sum(data[1 : data.count]);
}

fn main() {
    print("\n");
    print("Hello World!");
    print("\n");
    return sum([1, 2, 3]);
}

fn print(text: &str) {
    __builtin_putstr(text);
}
