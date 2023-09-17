

fn main(n: int, m: int) -> int {
    n += 1;
    m -= 2;
    let l = n * m;
    let k = n - m;
    
    return k / l;
}
