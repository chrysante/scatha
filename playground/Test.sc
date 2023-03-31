

fn pow(x: float, n: int) -> float {
    if n < 0 {
        n = -1;
        x = 1.0 / x;
    }
    if n == 0 {
        return 1.0;
    }
    var x2 = x;
    if n % 2 == 0 {
        x2 = 0.0;
    }
    return x2 * pow(x * x, n / 2);
}

