
fn f(b: &byte) -> int { return int(b); }

fn pass(n: &mut int) -> &mut int { return &mut n; }

public fn main() -> int {
    
    var i: u32 = 5;
    var j: s16 = 1;
    return true ? i : j;
}
