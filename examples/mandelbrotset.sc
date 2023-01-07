fn main() -> int {
    let width = 232.0;
    let height = 40.0;
    let scale = 2.0;
    for j = 0.0; j < height; j += 1.0 {
        for i = 0.0; i < width; i += 1.0 {
            var z: Complex;
            z.x = 2.0 / scale * ((i / width) - 0.5) * width / (2.0 * height);
            z.y = 2.0 / scale * ((1.0 - j / height) - 0.5);


            printDot(sqrt(mandelbrotSet(z)));
        }    
        printNewLine();
    }
    return 0;
}

fn pow(base: float, exp: int) -> float {
    if exp < 0 { return pow(1.0 / base, -exp); }
    if exp == 0 { return 1.0; }
    if exp == 1 { return base; }
    if exp % 2 == 1 { return base * pow(base, exp - 1); }
    return pow(base * base, exp / 2);
}

struct Complex {
    var x: float;
    var y: float;
}

fn mandelbrotSet(c: Complex) -> float {
    var z: Complex;
    z.x = 0.0;
    z.y = 0.0; 
    let limit = 90.0;
    for i = 0.0; i < limit; i += 1.0 {
        z = next(z, c);
        if length(z) > 2.0 {
            return i / limit;
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

fn length(z: Complex) -> float {
    return sqrt(z.x * z.x + z.y * z.y);
}

fn calcDot() -> float {
    return 0.5;
}

fn printDot(x: float) {
    if x < 0.2 {
        __builtin_putchar(32); // ' '
    }
    else if x < 0.4 {
        __builtin_putchar(46); // '.'
    }
    else if x < 0.6 {
        __builtin_putchar(45); // '-'
    }
    else if x < 0.8 {
        __builtin_putchar(42); // '*'
    }
    else {
        __builtin_putchar(35); // '#'
    }
}

fn printNewLine() {
    __builtin_putchar(10); // 10 == '\n'
}

fn print(x: int) {
    __builtin_puti64(x);
    printNewLine();
}

fn print(x: float) {
    __builtin_putf64(x);
    printNewLine();
}

fn sqrt(x: float) -> float {
    return __builtin_sqrtf64(x);
}