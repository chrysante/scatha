

fn main(n: int, m: int, data: *[int]) -> int {
    __builtin_puti64(n);
    n += 12 / m;
    m -= 2;
    let l = n * m;
    let k = n - m;
    
    return k / l + (*data)[m * n];
}
