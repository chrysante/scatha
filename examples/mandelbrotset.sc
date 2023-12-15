//! Compile with: scatha-c mandelbrotset.sc
//! Run with:     svm mandelbrotset.sbin

fn reportUsageError() {
    print("Usage: <width> <height>\n");
    print("       <width> <height> <search-depth>\n");
    __builtin_trap(); // For now because we don't have terminate() yet
}

fn parseInt(text: &str) {
    var result = 0;
    if !__builtin_strtos64(result, text, 10) {
        reportUsageError();
    }
    return result;
}

struct Options {
    var width: int;
    var height: int;
    var depth: int;

    fn parse(args: &[*str]) -> Options {
        if args.empty {
            return Options(160, 60, 200);
        }
        if args.count == 2 {
            return Options(parseInt(*args[0]), 
                           parseInt(*args[1]), 
                           200);
        }
        if args.count == 3 {
            return Options(parseInt(*args[0]), 
                           parseInt(*args[1]),
                           parseInt(*args[2])); 
        }
        reportUsageError();
    }
}

fn main(args: &[*str]) -> int {
    let options = Options.parse(args);
    let width = options.width;
    let height = options.height;
    let scale = 1.0;
    for j = 0; j < height; ++j {
        for i = 0; i < width; ++i {
            var z = Complex(
                2.0 / scale * ((double(i) / double(width)) - 0.5) * double(width) / (2.0 * double(height)),
                2.0 / scale * ((1.0 - double(j) / double(height)) - 0.5));
            let shade = toAsciiShade(sqrt(mandelbrotSet(z, options.depth)));
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

fn mandelbrotSet(c: Complex, limit: int) -> double {
    var z = Complex();
    for i = 0; i < limit; ++i {
        z = next(z, c);
        if z.length() > 2.0 {
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

fn sqrt(x: double) -> double {
    return __builtin_sqrt_f64(x);
}

fn toAsciiShade(x: double) -> byte {
    return " .-:=+*#%@"[int(10.0 * clamp(x, 0.0, 1.0))];
}

fn print(b: byte) {
    __builtin_putchar(b);
}

/// We'll just leave these print functions here for future debugging
fn print(z: double) {
    __builtin_putf64(z);
    __builtin_putstr("\n");
}

fn print(t: &str) {
    __builtin_putstr(t);
}

fn print(z: Complex) {
    __builtin_putstr("(");
    __builtin_putf64(z.x);
    __builtin_putstr(", ");
    __builtin_putf64(z.y);
    __builtin_putstr(")\n");
}
