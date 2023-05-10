//! Compile with: ./scatha-c -f mandelbrotset.sc
//! Run with:     ./svm -f mandelbrotset.sbin

public fn main() -> int {
    let width = 160;
    let height = 60;
    let scale = 1.0;
    for j = 0; j < height; ++j {
        for i = 0; i < width; ++i {
            var z: Complex;
            z.x = 2.0 / scale * ((double(i) / double(width)) - 0.5) * double(width) / (2.0 * double(height));
            z.y = 2.0 / scale * ((1.0 - double(j) / double(height)) - 0.5);
            printDot(sqrt(mandelbrotSet(z)));
        }    
        printNewLine();
    }
    return 0;
}

struct Complex {
    var x: double;
    var y: double;
}

fn mandelbrotSet(c: Complex) -> double {
    var z: Complex;
    z.x = 0.0;
    z.y = 0.0; 
    let limit = 200;
    for i = 0; i < limit; ++i {
        z = next(z, c);
        if length(z) > 2.0 {
            return double(i) / double(limit);
        }
    }
    return 0.0;
}

fn next(z: Complex, c: Complex) -> Complex {
    var r: Complex;
    r.x = z.x * z.x - z.y * z.y;
    r.y = 2.0 * z.x * z.y;
    r.x += c.x;
    r.y += c.y;
    return r;
}

fn length(z: Complex) -> double {
    return __builtin_hypot_f64(z.x, z.y);
}

fn calcDot() -> double {
    return 0.5;
}

fn printDot(x: double) {
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

fn print(x: int) {
    __builtin_puti64(x);
    printNewLine();
}

fn print(x: double) {
    __builtin_putf64(x);
    printNewLine();
}

fn sqrt(x: double) -> double {
    return __builtin_sqrt_f64(x);
}
