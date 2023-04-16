public fn main() -> int {
    let width = 160;
    let height = 60;
    let scale = 1.0;
    for j = 0; j < height; ++j {
        for i = 0; i < width; ++i {
            let fi = float(i);
            let fw = float(width);
            let fj = float(j);
            let fh = float(height);
            let x = fi / fw;
            let y = fj / fh;
            let val = x * y;
            printDot(val);
        }    
        printNewLine();
    }
    return 0;
}

fn printDot(x: float) {
         if x < 0.1 { __builtin_putchar(32); }
    else if x < 0.2 { __builtin_putchar(46); }
    else if x < 0.3 { __builtin_putchar(58); }
    else if x < 0.4 { __builtin_putchar(45); }
    else if x < 0.5 { __builtin_putchar(61); }
    else if x < 0.6 { __builtin_putchar(43); }
    else if x < 0.7 { __builtin_putchar(42); }
    else if x < 0.8 { __builtin_putchar(35); }
    else if x < 0.9 { __builtin_putchar(37); }
    else            { __builtin_putchar(64); }
}

fn printNewLine() {
    __builtin_putchar(10); // 10 == '\n'
}
