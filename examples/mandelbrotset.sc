//! Compile with: scatha-c mandelbrotset.sc
//! Run with:     svm mandelbrotset.sbin

fn main() -> int {
    let width = 160;
    let height = 60;
    let scale = 1.0;
    for j = 0; j < height; ++j {
        for i = 0; i < width; ++i {
            var z = Complex(
                2.0 / scale * ((double(i) / double(width)) - 0.5) * double(width) / (2.0 * double(height)),
                2.0 / scale * ((1.0 - double(j) / double(height)) - 0.5));
            let shade = toAsciiShade(sqrt(mandelbrotSet(z)));
            print(shade);
        }    
        print('\n');
    }
    return 0;
}

struct Complex {
    fn new(&mut this) {
        this.x = 0.0;
        this.y = 0.0;
    }

    fn new(&mut this, x: double, y: double) {
        this.x = x;
        this.y = y;
    }

    fn length(&this) -> double {
        return __builtin_hypot_f64(this.x, this.y);
    }

    var x: double;
    var y: double;
}

fn mandelbrotSet(c: Complex) -> double {
    var z = Complex();
    let limit = 200;
    for i = 0; i < limit; ++i {
        z = next(z, c);
        if z.length > 2.0 {
            return double(i) / double(limit);
        }
    }
    return 0.0;
}

fn next(z: Complex, c: Complex) -> Complex {
    return Complex(z.x * z.x - z.y * z.y + c.x,
                   2.0 * z.x * z.y       + c.y);
}

fn clamp(x: double, min: double, max: double) -> double {
    return x < min ? min : x > max ? max : x;
}

fn toAsciiShade(x: double) -> byte {
    return " .-:=+*#%@"[int(10.0 * clamp(x, 0.0, 1.0))];
}

fn print(b: byte) {
    __builtin_putchar(b);
}

fn sqrt(x: double) -> double {
    return __builtin_sqrt_f64(x);
}
