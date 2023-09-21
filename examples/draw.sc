fn main() -> int {
    let width = 160;
    let height = 60;
    let scale = 1.0;
    for j = 0; j < height; ++j {
        for i = 0; i < width; ++i {
            let fi = double(i);
            let fw = double(width);
            let fj = double(j);
            let fh = double(height);
            let x = fi / fw;
            let y = fj / fh;
            let val = x * y;
            __builtin_putchar(toAsciiShade(val));
        }    
        printNewLine();
    }
    return 0;
}

fn clamp(x: double, min: double, max: double) -> double {
    return x < min ? min : x > max ? max : x;
}

fn toAsciiShade(x: double) -> byte {
    return " .-:=+*#%@"[int(10.0 * clamp(x, 0.0, 1.0))];
}

fn printNewLine() {
    __builtin_putchar(10); // 10 == '\n'
}
