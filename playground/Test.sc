
public fn clamp(val: float, min: float, max: float) -> float {
    return val < min ? min : val > max ? max : val;
}

fn main() -> float {
    let f: float = 1;
    // return clamp(123.0, 0, 1);
}
