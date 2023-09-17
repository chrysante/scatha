
fn main(n: int, m: int, data: *[int]) -> int {
    __builtin_puti64(n);
    var cond = n == m;
    n += m == 1 ? 1:0;
    n += 12 / m;
    m -= 2;
    let l = n * m;
    let k = n - m;
    if cond {
        return l / k + n;
    }
    return k / l + (*data)[m * n];
}
