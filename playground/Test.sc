

struct X {
    fn sum(&this) -> int {
        var res = 0;
        for i = 0; i < this.r.count; ++i {
            res += this.r[i];
        }
        return res;
    }
    var x: int;
    var r: &[int];
}

public fn main() -> int {
    let a = [1, 2, 3];
    var x: X;
    x.r = &a;
    return x.sum();
}
